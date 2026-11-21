#include "rounds/OcrRoundDetectorConfigLoader.h"

#include <charconv>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "util/Json.h"

namespace tft {
namespace {

std::string Trim(std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return std::string(value.substr(first, last - first + 1));
}

std::vector<std::string> SplitAndTrim(std::string_view value, char separator) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= value.size()) {
        const auto pos = value.find(separator, start);
        const auto chunk = value.substr(start, pos == std::string_view::npos
                                                   ? std::string_view::npos
                                                   : pos - start);
        auto trimmed = Trim(chunk);
        if (!trimmed.empty()) parts.push_back(std::move(trimmed));
        if (pos == std::string_view::npos) break;
        start = pos + 1;
    }
    return parts;
}

bool ParseDouble(std::string_view text, double& out) {
    const auto value = Trim(text);
    if (value.empty()) return false;
    const char* begin = value.data();
    const char* end = begin + value.size();
    const auto result = std::from_chars(begin, end, out);
    return result.ec == std::errc{} && result.ptr == end;
}

bool ParseInt(std::string_view text, int& out) {
    const auto value = Trim(text);
    if (value.empty()) return false;
    const char* begin = value.data();
    const char* end = begin + value.size();
    const auto result = std::from_chars(begin, end, out);
    return result.ec == std::errc{} && result.ptr == end;
}

const std::string* FindString(const json::Object& obj, const std::string& key) {
    const auto it = obj.find(key);
    if (it == obj.end() || it->second.type != json::ValueType::String) return nullptr;
    return &it->second.text;
}

bool ValidRect(const NormalizedRect& band) {
    return band.x >= 0.0 && band.y >= 0.0 && band.w > 0.0 && band.h > 0.0
        && band.x + band.w <= 1.0 && band.y + band.h <= 1.0;
}

} // namespace

OcrRoundDetectorConfig DefaultOcrRoundDetectorConfig() { return OcrRoundDetectorConfig{}; }

std::string OcrRoundDetectorConfigSampleJson() {
    return R"({
  "_comment": "Faixa normalizada (x,y,w,h) do topo da janela onde o Windows OCR procura o contador. Deve cobrir a barra expandida e a compacta.",
  "band": "0.15,0.0,0.70,0.18",
  "sample_interval_ms": "100",
  "warmup_ms": "500",
  "confirmation_samples": "2",
  "confirmation_window_ms": "15000",
  "cooldown_ms": "1200",
  "source": "hud-ocr"
}
)";
}

bool ParseOcrRoundDetectorConfigJson(const std::string& json,
                                     OcrRoundDetectorConfig& out,
                                     std::string* error) {
    json::Object obj;
    if (!json::ParseFlatObject(json, obj, error)) return false;

    OcrRoundDetectorConfig cfg = out;
    if (const auto* band = FindString(obj, "band")) {
        const auto values = SplitAndTrim(*band, ',');
        if (values.size() != 4
            || !ParseDouble(values[0], cfg.band.x)
            || !ParseDouble(values[1], cfg.band.y)
            || !ParseDouble(values[2], cfg.band.w)
            || !ParseDouble(values[3], cfg.band.h)
            || !ValidRect(cfg.band)) {
            if (error) *error = "band inválida (use x,y,w,h entre 0 e 1)";
            return false;
        }
    }
    if (const auto* value = FindString(obj, "sample_interval_ms")) {
        if (!ParseInt(*value, cfg.sample_interval_ms) || cfg.sample_interval_ms < 50
            || cfg.sample_interval_ms > 5000) {
            if (error) *error = "sample_interval_ms inválido (50..5000)";
            return false;
        }
    }
    if (const auto* value = FindString(obj, "warmup_ms")) {
        if (!ParseInt(*value, cfg.warmup_ms) || cfg.warmup_ms < 0
            || cfg.warmup_ms > 600'000) {
            if (error) *error = "warmup_ms inválido (0..600000)";
            return false;
        }
    }
    if (const auto* value = FindString(obj, "confirmation_samples")) {
        if (!ParseInt(*value, cfg.confirmation_samples) || cfg.confirmation_samples < 1
            || cfg.confirmation_samples > 10) {
            if (error) *error = "confirmation_samples inválido (1..10)";
            return false;
        }
    }
    if (const auto* value = FindString(obj, "confirmation_window_ms")) {
        if (!ParseInt(*value, cfg.confirmation_window_ms) || cfg.confirmation_window_ms < 1000
            || cfg.confirmation_window_ms > 300'000) {
            if (error) *error = "confirmation_window_ms inválido (1000..300000)";
            return false;
        }
    }
    if (const auto* value = FindString(obj, "cooldown_ms")) {
        if (!ParseInt(*value, cfg.cooldown_ms) || cfg.cooldown_ms < 0
            || cfg.cooldown_ms > 600'000) {
            if (error) *error = "cooldown_ms inválido (0..600000)";
            return false;
        }
    }
    if (const auto* source = FindString(obj, "source")) {
        if (Trim(*source).empty()) {
            if (error) *error = "source não pode ser vazio";
            return false;
        }
        cfg.source = Trim(*source);
    }

    out = std::move(cfg);
    return true;
}

bool LoadOcrRoundDetectorConfig(const std::string& path,
                                OcrRoundDetectorConfig& out,
                                std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error) *error = "Não foi possível abrir " + path;
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ParseOcrRoundDetectorConfigJson(ss.str(), out, error);
}

std::string FindOcrRoundDetectorConfigPath() {
    const char* candidates[] = {
        "tft_round_detector.json",
        "config/tft_round_detector.json",
        "recorder/config/tft_round_detector.json",
    };
    for (const char* path : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(path, ec)) return path;
    }
    return {};
}

} // namespace tft
