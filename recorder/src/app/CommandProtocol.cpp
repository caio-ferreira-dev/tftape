#include "app/CommandProtocol.h"

#include <initializer_list>
#include <sstream>

#include "util/Json.h"

namespace tft {
namespace {

std::string JsonEscape(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

bool OnlyKnownKeys(const json::Object& obj, std::initializer_list<std::string_view> allowed,
                   std::string* error) {
    for (const auto& [key, field] : obj) {
        (void)field;
        bool known = false;
        for (auto name : allowed) {
            if (std::string_view(key) == name) {
                known = true;
                break;
            }
        }
        if (!known) {
            if (error) *error = "campo desconhecido: '" + key + "'";
            return false;
        }
    }
    return true;
}

bool ReadString(const json::Object& obj, std::string_view key, std::string& out, std::string* error) {
    const auto name = std::string(key);
    auto it = obj.find(name);
    if (it == obj.end()) {
        if (error) *error = "campo obrigatório ausente: '" + name + "'";
        return false;
    }
    if (it->second.type != json::ValueType::String) {
        if (error) *error = "campo '" + name + "' deve ser string";
        return false;
    }
    out = it->second.text;
    return true;
}

bool ReadBool(const json::Object& obj, std::string_view key, bool fallback, bool& out,
              std::string* error) {
    const auto name = std::string(key);
    auto it = obj.find(name);
    if (it == obj.end()) {
        out = fallback;
        return true;
    }
    if (it->second.type != json::ValueType::Bool) {
        if (error) *error = "campo '" + name + "' deve ser booleano (true/false)";
        return false;
    }
    out = it->second.bool_value;
    return true;
}

} // namespace

std::string Event::ToJsonLine() const {
    std::ostringstream oss;
    oss << "{\"event\":\"" << JsonEscape(name) << "\","
        << "\"ok\":" << (ok ? "true" : "false") << ","
        << "\"message\":\"" << JsonEscape(message) << "\","
        << "\"path\":\"" << JsonEscape(path) << "\"";
    if (!sidecar_path.empty()) {
        oss << ",\"sidecar_path\":\"" << JsonEscape(sidecar_path) << "\"";
    }
    if (duration_ms >= 0) {
        oss << ",\"duration_ms\":" << duration_ms;
    }
    oss << '}';
    return oss.str();
}

Event CommandProtocol::Ok(std::string_view name, std::string_view message, std::string_view path) {
    return Event{std::string(name), std::string(message), std::string(path), true, "", -1};
}

Event CommandProtocol::Error(std::string_view name, std::string_view message) {
    return Event{std::string(name), std::string(message), "", false, "", -1};
}

std::optional<Command> CommandProtocol::ParseLine(std::string_view line) {
    return ParseLine(line, nullptr);
}

std::optional<Command> CommandProtocol::ParseLine(std::string_view line, std::string* error) {
    json::Object obj;
    if (!json::ParseFlatObject(line, obj, error)) return std::nullopt;

    std::string cmd;
    if (!ReadString(obj, "cmd", cmd, error)) return std::nullopt;

    if (cmd == "stop" || cmd == "ping" || cmd == "shutdown") {
        if (!OnlyKnownKeys(obj, {"cmd"}, error)) return std::nullopt;
        Command c;
        if (cmd == "stop") c.type = CommandType::Stop;
        else if (cmd == "ping") c.type = CommandType::Ping;
        else c.type = CommandType::Shutdown;
        return c;
    }

    if (cmd != "start") {
        if (error) *error = "comando desconhecido: '" + cmd + "'";
        return std::nullopt;
    }

    if (!OnlyKnownKeys(obj, {"cmd", "output_dir", "resolution", "fps", "game_audio",
                             "system_audio", "microphone"}, error)) {
        return std::nullopt;
    }

    Command c;
    c.type = CommandType::Start;
    if (!ReadString(obj, "output_dir", c.settings.output_dir, error)) return std::nullopt;

    auto it = obj.find("resolution");
    if (it != obj.end()) {
        if (it->second.type != json::ValueType::String) {
            if (error) *error = "campo 'resolution' deve ser string";
            return std::nullopt;
        }
        if (!ParseResolution(it->second.text, c.settings.resolution)) {
            if (error) *error = "resolução inválida: '" + it->second.text + "' (use 720p ou 1080p)";
            return std::nullopt;
        }
    }

    it = obj.find("fps");
    if (it != obj.end()) {
        if (it->second.type != json::ValueType::String) {
            if (error) *error = "campo 'fps' deve ser string";
            return std::nullopt;
        }
        if (!ParseFramerate(it->second.text, c.settings.framerate)) {
            if (error) *error = "fps inválido: '" + it->second.text + "' (use 30 ou 60)";
            return std::nullopt;
        }
    }

    if (!ReadBool(obj, "game_audio", true, c.settings.audio.game_audio, error)) return std::nullopt;
    if (!ReadBool(obj, "system_audio", false, c.settings.audio.system_audio, error)) return std::nullopt;
    if (!ReadBool(obj, "microphone", false, c.settings.audio.microphone, error)) return std::nullopt;
    return c;
}

} // namespace tft
