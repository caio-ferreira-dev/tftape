#pragma once

#include <string>
#include <string_view>

namespace tft {

/// Normaliza o texto devolvido pelo OCR: unifica os vários tipos de hífen,
/// descarta letras/pontuação e converte confusões comuns (I/l/L -> 1).
/// Exemplo: "ROUND 1 – 1" -> "1-1".
std::string NormalizeOcrRoundText(std::string_view text);

/// Encontra o primeiro contador válido (1-1 ... 9-9) em qualquer parte do texto.
bool TryParseRoundLabel(std::string_view text, std::string& label);

/// Só aceita se o texto BRUTO contém apenas o contador (dígitos, espaços e
/// hífens). Usado para preferir linhas isoladas em vez de texto decorativo.
bool ParseExactRoundLabel(std::string_view text, std::string& label);

} // namespace tft
