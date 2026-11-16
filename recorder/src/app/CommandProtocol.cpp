#include "app/CommandProtocol.h"

#include <cctype>
#include <sstream>

namespace tft {

static std::string JsonEscape(std::string_view s) {
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

std::string Event::ToJsonLine() const {
    std::ostringstream oss;
    oss << "{\"event\":\"" << JsonEscape(name) << "\","
        << "\"ok\":" << (ok ? "true" : "false") << ","
        << "\"message\":\"" << JsonEscape(message) << "\","
        << "\"path\":\"" << JsonEscape(path) << "\"}";
    return oss.str();
}

Event CommandProtocol::Ok(std::string_view name, std::string_view message, std::string_view path) {
    return Event{std::string(name), std::string(message), std::string(path), true};
}
Event CommandProtocol::Error(std::string_view name, std::string_view message) {
    return Event{std::string(name), std::string(message), "", false};
}

std::optional<std::string> CommandProtocol::ExtractString(std::string_view json, std::string_view key) {
    std::string pattern = "\"" + std::string(key) + "\"";
    auto pos = json.find(pattern);
    if (pos == std::string::npos) return std::nullopt;
    pos = json.find(':', pos + pattern.size());
    if (pos == std::string::npos) return std::nullopt;
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return std::nullopt;
    auto end = pos + 1;
    std::string value;
    while (end < json.size()) {
        char c = json[end++];
        if (c == '\\' && end < json.size()) {
            char n = json[end++];
            if (n == 'n') value.push_back('\n');
            else if (n == 'r') value.push_back('\r');
            else if (n == 't') value.push_back('\t');
            else value.push_back(n);
        } else if (c == '"') {
            return value;
        } else {
            value.push_back(c);
        }
    }
    return std::nullopt;
}

std::optional<bool> CommandProtocol::ExtractBool(std::string_view json, std::string_view key) {
    std::string pattern = "\"" + std::string(key) + "\"";
    auto pos = json.find(pattern);
    if (pos == std::string::npos) return std::nullopt;
    pos = json.find(':', pos + pattern.size());
    if (pos == std::string::npos) return std::nullopt;
    auto rest = json.substr(pos + 1);
    auto t = rest.find("true");
    auto f = rest.find("false");
    if (t != std::string_view::npos && (f == std::string_view::npos || t < f)) return true;
    if (f != std::string_view::npos) return false;
    return std::nullopt;
}

std::optional<Command> CommandProtocol::ParseLine(std::string_view line) {
    if (line.empty()) return std::nullopt;
    auto cmd = ExtractString(line, "cmd");
    if (!cmd) return std::nullopt;
    Command c;
    if (*cmd == "start") {
        c.type = CommandType::Start;
        if (auto dir = ExtractString(line, "output_dir")) c.settings.output_dir = *dir;
        if (auto res = ExtractString(line, "resolution")) {
            Resolution r; if (ParseResolution(*res, r)) c.settings.resolution = r;
        }
        if (auto fps = ExtractString(line, "fps")) {
            Framerate f; if (ParseFramerate(*fps, f)) c.settings.framerate = f;
        }
        if (auto v = ExtractBool(line, "game_audio")) c.settings.audio.game_audio = *v;
        if (auto v = ExtractBool(line, "system_audio")) c.settings.audio.system_audio = *v;
        if (auto v = ExtractBool(line, "microphone")) c.settings.audio.microphone = *v;
        return c;
    } else if (*cmd == "stop") {
        c.type = CommandType::Stop; return c;
    } else if (*cmd == "ping") {
        c.type = CommandType::Ping; return c;
    } else if (*cmd == "shutdown") {
        c.type = CommandType::Shutdown; return c;
    }
    c.type = CommandType::Unknown;
    return c;
}

} // namespace tft
