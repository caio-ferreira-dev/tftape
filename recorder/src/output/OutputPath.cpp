#include "output/OutputPath.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>

namespace tft {
namespace {

bool IsDriveAbsolute(std::string_view s) {
    return s.size() >= 3 &&
           std::isalpha(static_cast<unsigned char>(s[0])) &&
           s[1] == ':' && (s[2] == '/' || s[2] == '\\');
}

bool IsUncAbsolute(std::string_view s) {
    return s.size() >= 2 && s[0] == '\\' && s[1] == '\\';
}

bool IsReservedDeviceName(std::string_view component) {
    const auto dot = component.find('.');
    const auto base = component.substr(0, dot);
    if (base.empty() || base.size() > 4) return false;

    std::string upper(base);
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    static constexpr std::string_view kReserved[] = {
        "CON", "PRN", "AUX", "NUL",
        "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
        "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
    };
    for (auto name : kReserved) {
        if (std::string_view(upper) == name) return true;
    }
    return false;
}

} // namespace

bool OutputPath::ValidateDirectorySyntax(std::string_view dir, std::string* error) {
    auto fail = [error](std::string message) {
        if (error) *error = std::move(message);
        return false;
    };

    if (dir.empty()) return fail("diretório de saída vazio");

    const bool drive = IsDriveAbsolute(dir);
    const bool unc = IsUncAbsolute(dir);
    const bool posix = dir.front() == '/';
    if (!drive && !unc && !posix) {
        return fail("caminho de saída deve ser absoluto (ex.: C:/Vods)");
    }

    const std::size_t start = drive ? 3 : (unc ? 2 : 1);
    int components = 0;
    for (std::size_t pos = start; pos < dir.size();) {
        auto end = dir.find_first_of("/\\", pos);
        if (end == std::string_view::npos) end = dir.size();
        const auto component = dir.substr(pos, end - pos);

        if (!component.empty()) {
            ++components;
            if (component == "." || component == "..") {
                return fail("componentes '.' e '..' não são permitidos no caminho de saída");
            }
            if (component.back() == '.' || component.back() == ' ') {
                return fail("componente do caminho termina com '.' ou espaço: '" +
                            std::string(component) + "'");
            }
            for (unsigned char c : component) {
                if (c < 32) return fail("caractere de controle no caminho de saída");
                if (c == '<' || c == '>' || c == ':' || c == '"' || c == '|' ||
                    c == '?' || c == '*') {
                    return fail("caractere inválido no caminho de saída: '" +
                                std::string(1, static_cast<char>(c)) + "'");
                }
            }
            if (IsReservedDeviceName(component)) {
                return fail("nome de dispositivo reservado no caminho: '" +
                            std::string(component) + "'");
            }
        }

        if (end == dir.size()) break;
        pos = end + 1;
    }

    if (unc && components < 2) {
        return fail("caminho UNC incompleto (use \\\\servidor\\compartilhamento)");
    }
    return true;
}

bool OutputPath::EnsureDirectory(const std::filesystem::path& dir, std::string* error) {
    std::string syntax_error;
    if (!ValidateDirectorySyntax(dir.string(), &syntax_error)) {
        if (error) *error = syntax_error;
        return false;
    }

    std::error_code ec;
    if (std::filesystem::exists(dir, ec)) {
        if (!std::filesystem::is_directory(dir, ec)) {
            if (error) *error = "caminho de saída não é um diretório";
            return false;
        }
    } else if (!std::filesystem::create_directories(dir, ec)) {
        if (error) *error = "falha ao criar diretório: " + ec.message();
        return false;
    }

    // Verifica permissão de escrita antes de iniciar a gravação (evita falhar só no mux).
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    for (int attempt = 0; attempt < 10; ++attempt) {
        auto probe = dir / (".tft_write_probe_" + std::to_string(ticks) + "_" +
                            std::to_string(attempt));
        if (std::filesystem::exists(probe, ec)) continue;
        std::ofstream file(probe, std::ios::binary);
        if (!file.is_open()) {
            if (error) *error = "sem permissão de escrita no diretório de saída";
            return false;
        }
        file.close();
        std::filesystem::remove(probe, ec);
        return true;
    }
    if (error) *error = "não foi possível verificar a permissão de escrita no diretório";
    return false;
}

std::string OutputPath::SanitizeFilename(std::string_view name) {
    std::string out;
    out.reserve(name.size());
    for (char c : name) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' ||
            c == '<' || c == '>' || c == '|') {
            out.push_back('_');
        } else if (static_cast<unsigned char>(c) < 32) {
            continue;
        } else {
            out.push_back(c);
        }
    }
    while (!out.empty() && (out.back() == '.' || out.back() == ' ')) out.pop_back();
    if (IsReservedDeviceName(out)) out.insert(out.begin(), '_');
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
