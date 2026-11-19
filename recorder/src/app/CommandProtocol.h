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

// Protocolo JSON-lines da UI. O parse é estrito: JSON malformado, tipos errados,
// chaves desconhecidas/duplicadas e valores inválidos são recusados com mensagem
// detalhada em `error` (quando fornecido), em vez de caírem silenciosamente no padrão.
class CommandProtocol {
public:
    static std::optional<Command> ParseLine(std::string_view line);
    static std::optional<Command> ParseLine(std::string_view line, std::string* error);
    static Event Ok(std::string_view name, std::string_view message = "", std::string_view path = "");
    static Event Error(std::string_view name, std::string_view message);
};

} // namespace tft
