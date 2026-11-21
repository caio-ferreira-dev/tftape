#include "rounds/RoundDetectorConfigLoader.h"

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

constexpr std::string_view kDefaultLabels =
    "1-1,1-2,1-3,1-4,2-1,2-2,2-3,2-4,2-5,2-6,2-7,"
    "3-1,3-2,3-3,3-4,3-5,3-6,3-7,4-1,4-2,4-3,4-4,4-5,4-6,4-7,"
    "5-1,5-2,5-3,5-4,5-5,5-6,5-7,6-1,6-2,6-3,6-4,6-5,6-6,6-7";

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

bool ValidRect(const NormalizedRect& roi) {
    return roi.x >= 0.0 && roi.y >= 0.0 && roi.w > 0.0 && roi.h > 0.0
        && roi.x + roi.w <= 1.0 && roi.y + roi.h <= 1.0;
}

} // namespace

RoundDetectorConfig DefaultRoundDetectorConfig() {
    RoundDetectorConfig config;
    config.roi = NormalizedRect{};
    config.labels = SplitAndTrim(kDefaultLabels, ',');
    return config;
}

std::string RoundDetectorConfigSampleJson() {
    return R"({
  "_comment": "ROI normalizada (x,y,w,h) do contador de round no HUD. Ajuste com um print do cliente.",
  "roi": "0.42,0.02,0.16,0.08",
  "labels": "1-1,1-2,1-3,1-4,2-1,2-2,2-3,2-4,2-5,2-6,2-7,3-1,3-2,3-3,3-4,3-5,3-6,3-7,4-1,4-2,4-3,4-4,4-5,4-6,4-7,5-1,5-2,5-3,5-4,5-5,5-6,5-7,6-1,6-2,6-3,6-4,6-5,6-6,6-7",
  "change_threshold": "14",
  "stability_frames": "4",
  "cooldown_ms": "2500",
  "warmup_ms": "3000",
  "emit_initial_round": "true",
  "source": "hud-counter"
}
)";
}

bool ParseRoundDetectorConfigJson(const std::string& json,
                                  RoundDetectorConfig& out,
                                  std::string* error) {
    json::Object obj;
    if (!json::ParseFlatObject(json, obj, error)) return false;

    RoundDetectorConfig cfg = out;
    if (const auto* roi = FindString(obj, "roi")) {
        const auto values = SplitAndTrim(*roi, ',');
        if (values.size() != 4
            || !ParseDouble(values[0], cfg.roi.x)
            || !ParseDouble(values[1], cfg.roi.y)
            || !ParseDouble(values[2], cfg.roi.w)
            || !ParseDouble(values[3], cfg.roi.h)
            || !ValidRect(cfg.roi)) {
            if (error) *error = "ROI inválida (use x,y,w,h entre 0 e 1)";
            return false;
        }
    }
    if (const auto* labels = FindString(obj, "labels")) {
        cfg.labels = SplitAndTrim(*labels, ',');
        if (cfg.labels.empty()) {
            if (error) *error = "labels não pode ser vazio";
            return false;
        }
    }
    if (const auto* value = FindString(obj, "change_threshold")) {
        if (!ParseInt(*value, cfg.change_threshold) || cfg.change_threshold < 1
            || cfg.change_threshold > 255) {
            if (error) *error = "change_threshold inválido (1..255)";
            return false;
        }
    }
    if (const auto* value = FindString(obj, "stability_frames")) {
        if (!ParseInt(*value, cfg.stability_frames) || cfg.stability_frames < 1
            || cfg.stability_frames > 600) {
            if (error) *error = "stability_frames inválido (1..600)";
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
    if (const auto* value = FindString(obj, "warmup_ms")) {
        if (!ParseInt(*value, cfg.warmup_ms) || cfg.warmup_ms < 0
            || cfg.warmup_ms > 600'000) {
            if (error) *error = "warmup_ms inválido (0..600000)";
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
    const auto initial = obj.find("emit_initial_round");
    if (initial != obj.end()) {
        if (initial->second.type == json::ValueType::Bool) {
            cfg.emit_initial_round = initial->second.bool_value;
        } else if (initial->second.type == json::ValueType::String) {
            const auto value = Trim(initial->second.text);
            if (value == "true" || value == "1" || value == "sim") {
                cfg.emit_initial_round = true;
            } else if (value == "false" || value == "0" || value == "nao") {
                cfg.emit_initial_round = false;
            } else {
                if (error) *error = "emit_initial_round inválido (true/false)";
                return false;
            }
        } else {
            if (error) *error = "emit_initial_round inválido (true/false)";
            return false;
        }
    }

    out = std::move(cfg);
    return true;
}

bool LoadRoundDetectorConfig(const std::string& path, RoundDetectorConfig& out, std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error) *error = "Não foi possível abrir " + path;
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ParseRoundDetectorConfigJson(ss.str(), out, error);
}

std::string FindRoundDetectorConfigPath() {
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
