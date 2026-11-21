#pragma once

#include <string>
#include <string_view>

namespace tft {

/// Normaliza o texto devolvido pelo OCR: unifica os vários tipos de hífen,
/// remove espaços e converte confusões comuns (I/l/L -> 1).
/// Exemplo: "ROUND 1 – 1" -> "ROUND1-1" (o contador "1-1" continua presente).
std::string NormalizeOcrRoundText(std::string_view text);

/// Encontra o primeiro contador válido (1-1 ... 9-9) em qualquer parte do texto.
bool TryParseRoundLabel(std::string_view text, std::string& label);

/// Como TryParseRoundLabel, mas exige que o texto normalizado seja exatamente N-M.
/// Usado para preferir linhas que contêm só o contador em vez de números soltos.
bool ParseExactRoundLabel(std::string_view text, std::string& label);

} // namespace tft
