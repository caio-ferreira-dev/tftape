#pragma once

#include <map>
#include <string>
#include <string_view>

namespace tft::json {

enum class ValueType { String, Bool, Other };

struct Field {
    ValueType type = ValueType::Other;
    std::string text;       // preenchido quando type == String (já decodificado, UTF-8)
    bool bool_value = false; // preenchido quando type == Bool
};

using Object = std::map<std::string, Field>;

// Analisa um objeto JSON estrito (chaves são strings; valores podem ser qualquer JSON).
// Valores que não são string/bool são marcados como Other: o chamador decide se os aceita.
// Rejeita JSON malformado, chaves duplicadas, vírgula final, escapes inválidos e lixo após '}'.
bool ParseFlatObject(std::string_view text, Object& out, std::string* error = nullptr);

} // namespace tft::json
