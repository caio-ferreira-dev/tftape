#include "output/RecordingSidecar.h"

#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <system_error>
#include <utility>

namespace tft {
namespace {

std::string JsonEscape(std::string_view text) {
    std::string out;
    out.reserve(text.size() + 8);
    for (unsigned char c : text) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    std::ostringstream oss;
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(c);
                    out += oss.str();
                } else {
                    out.push_back(static_cast<char>(c)); // UTF-8 passa direto.
                }
                break;
        }
    }
    return out;
}

bool Validate(const RecordingSidecarData& data, std::string& error) {
    auto fail = [&error](std::string message) {
        error = std::move(message);
        return false;
    };

    if (data.schemaVersion <= 0) return fail("schemaVersion inválido no sidecar");
    if (data.videoFile.empty()) return fail("nome do vídeo vazio no sidecar");
    if (data.createdAtUtc.empty()) return fail("createdAtUtc vazio no sidecar");
    if (data.durationMs < 0) return fail("durationMs negativo no sidecar");
    if (data.resolution.empty()) return fail("resolução vazia no sidecar");
    if (data.fps <= 0) return fail("fps inválido no sidecar");

    for (const auto& round : data.rounds) {
        if (round.label.empty()) return fail("round sem rótulo no sidecar");
        if (round.startMs < 0) return fail("startMs negativo no round '" + round.label + "'");
        if (data.durationMs > 0 && round.startMs > data.durationMs) {
            return fail("startMs do round '" + round.label + "' além da duração do vídeo");
        }
    }
    return true;
}

std::string BuildJson(const RecordingSidecarData& data) {
    std::ostringstream oss;
    oss.imbue(std::locale::classic());

    oss << "{\"schemaVersion\":" << data.schemaVersion
        << ",\"videoFile\":\"" << JsonEscape(data.videoFile) << "\""
        << ",\"createdAtUtc\":\"" << JsonEscape(data.createdAtUtc) << "\""
        << ",\"durationMs\":" << data.durationMs
        << ",\"resolution\":\"" << JsonEscape(data.resolution) << "\""
        << ",\"fps\":" << data.fps
        << ",\"rounds\":[";

    for (std::size_t i = 0; i < data.rounds.size(); ++i) {
        const auto& round = data.rounds[i];
        if (i != 0) oss << ',';
        oss << "{\"label\":\"" << JsonEscape(round.label) << "\""
            << ",\"startMs\":" << round.startMs
            << ",\"source\":\"" << JsonEscape(round.source) << "\""
            << ",\"confidence\":" << round.confidence << '}';
    }

    oss << "]}";
    return oss.str();
}

} // namespace

std::filesystem::path SidecarPathFor(const std::filesystem::path& mp4Path) {
    auto path = mp4Path;
    path += ".tftape.json";
    return path;
}

bool WriteRecordingSidecar(const RecordingSidecarData& data,
                           const std::filesystem::path& mp4Path,
                           std::string& error) {
    if (!Validate(data, error)) return false;

    const auto finalPath = SidecarPathFor(mp4Path);
    auto temporaryPath = finalPath;
    temporaryPath += ".tmp";

    std::error_code ec;
    const auto parent = finalPath.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        ec.clear();
    }

    {
        std::ofstream out(temporaryPath, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            error = "não foi possível criar o arquivo temporário do sidecar";
            return false;
        }
        out << BuildJson(data);
        out.flush();
        if (!out) {
            error = "falha ao escrever o sidecar";
            std::filesystem::remove(temporaryPath, ec);
            return false;
        }
    }

    // No Windows, rename não substitui um arquivo existente: remove antes (best effort).
    std::filesystem::remove(finalPath, ec);
    ec.clear();
    std::filesystem::rename(temporaryPath, finalPath, ec);
    if (ec) {
        std::filesystem::remove(temporaryPath, ec);
        error = "falha ao finalizar o sidecar: " + ec.message();
        return false;
    }

    error.clear();
    return true;
}

} // namespace tft
