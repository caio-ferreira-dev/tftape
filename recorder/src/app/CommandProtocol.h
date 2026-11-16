#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "output/RecordingSettings.h"

namespace tft {

enum class CommandType { Start, Stop, Ping, Shutdown, Unknown };

struct Command {
    CommandType type = CommandType::Unknown;
    RecordingSettings settings{}; // usado em Start
};

struct Event {
    std::string name;
    std::string message;
    std::string path;
    bool ok = true;

    std::string ToJsonLine() const;
};

// Protocolo JSON-lines simples (campos conhecidos). Suficiente para controle UI<->core.
class CommandProtocol {
public:
    static std::optional<Command> ParseLine(std::string_view line);
    static Event Ok(std::string_view name, std::string_view message = "", std::string_view path = "");
    static Event Error(std::string_view name, std::string_view message);

    static std::optional<std::string> ExtractString(std::string_view json, std::string_view key);
    static std::optional<bool> ExtractBool(std::string_view json, std::string_view key);
};

} // namespace tft
