#include "rounds/OcrRoundText.h"

#include <cstdint>

namespace tft {
namespace {

bool IsDigit(char c) { return c >= '0' && c <= '9'; }

int ToDigit(char c) { return c - '0'; }

bool IsDashCodePoint(std::uint32_t cp) {
    switch (cp) {
        case 0x002D: // hífen
        case 0x2010: // hyphen
        case 0x2011: // non-breaking hyphen
        case 0x2012: // figure dash
        case 0x2013: // en dash
        case 0x2014: // em dash
        case 0x2015: // horizontal bar
        case 0x2212: // minus sign
        case 0xFF0D: // fullwidth hyphen-minus
            return true;
        default:
            return false;
    }
}

bool IsSpaceCodePoint(std::uint32_t cp) {
    return cp == 0x0020 || cp == 0x0009 || cp == 0x000A || cp == 0x000D
        || cp == 0x00A0 || (cp >= 0x2000 && cp <= 0x200A);
}

/// Decodifica um ponto de código UTF-8. Caracteres inválidos são ignorados
/// pelo chamador (cp = 0xFFFD).
bool NextCodePoint(std::string_view text, std::size_t& i, std::uint32_t& cp) {
    if (i >= text.size()) return false;

    const auto lead = static_cast<unsigned char>(text[i]);
    std::size_t len = 0;
    if (lead < 0x80) {
        cp = lead;
        len = 1;
    } else if ((lead & 0xE0) == 0xC0) {
        cp = lead & 0x1F;
        len = 2;
    } else if ((lead & 0xF0) == 0xE0) {
        cp = lead & 0x0F;
        len = 3;
    } else if ((lead & 0xF8) == 0xF0) {
        cp = lead & 0x07;
        len = 4;
    } else {
        cp = 0xFFFD;
        ++i;
        return true;
    }

    if (i + len > text.size()) {
        cp = 0xFFFD;
        ++i;
        return true;
    }
    for (std::size_t j = 1; j < len; ++j) {
        const auto next = static_cast<unsigned char>(text[i + j]);
        if ((next & 0xC0) != 0x80) {
            len = 0;
            break;
        }
        cp = (cp << 6) | (next & 0x3F);
    }
    if (len == 0) {
        cp = 0xFFFD;
        ++i;
        return true;
    }
    i += len;
    return true;
}

bool ParsePattern(std::string_view normalized, std::string& label, bool exact) {
    for (std::size_t dash = 0; dash < normalized.size(); ++dash) {
        if (normalized[dash] != '-') continue;

        // Estágio: todos os dígitos contíguos antes do hífen (1 ou 2 algarismos).
        std::size_t begin = dash;
        while (begin > 0 && (dash - begin) < 2 && IsDigit(normalized[begin - 1])) --begin;
        if (begin == dash) continue;
        if (begin > 0 && IsDigit(normalized[begin - 1])) continue;
        if (exact && begin != 0) continue;

        int stage_number = 0;
        for (std::size_t k = begin; k < dash; ++k) {
            stage_number = stage_number * 10 + ToDigit(normalized[k]);
        }
        if (stage_number < 1 || stage_number > 9) continue;

        // Rodada: tenta 1 algarismo antes de 2. Isso recupera leituras como
        // "1-10" (contador "1-1" colado a um "0" vizinho do HUD).
        bool matched = false;
        for (std::size_t round_len = 1; round_len <= 2 && !matched; ++round_len) {
            if (dash + round_len >= normalized.size()) break;
            int round_number = 0;
            bool valid = true;
            for (std::size_t k = 1; k <= round_len; ++k) {
                const char c = normalized[dash + k];
                if (!IsDigit(c)) { valid = false; break; }
                round_number = round_number * 10 + ToDigit(c);
            }
            if (!valid || round_number < 1 || round_number > 9) continue;
            if (exact && dash + round_len + 1 != normalized.size()) continue;

            label = std::to_string(stage_number) + "-" + std::to_string(round_number);
            matched = true;
        }
        if (matched) return true;
    }
    return false;
}

} // namespace

std::string NormalizeOcrRoundText(std::string_view text) {
    std::string out;
    bool last_was_sep = true; // início conta como separador

    std::size_t i = 0;
    std::uint32_t cp = 0;
    while (NextCodePoint(text, i, cp)) {
        if (cp >= '0' && cp <= '9') {
            out.push_back(static_cast<char>(cp));
            last_was_sep = false;
        } else if (IsDashCodePoint(cp)) {
            // Espaços ao redor do hífen fazem parte dele, não viram separador.
            if (!out.empty() && out.back() == '|') out.pop_back();
            out.push_back('-');
            last_was_sep = true;
        } else if (cp == 'I' || cp == 'i' || cp == 'l' || cp == 'L') {
            out.push_back('1');
            last_was_sep = false;
        } else if (!last_was_sep) {
            out.push_back('|');
            last_was_sep = true;
        }
    }

    while (!out.empty() && (out.back() == '|' || out.back() == '-')) out.pop_back();
    const auto first = out.find_first_not_of('|');
    if (first == std::string::npos) return {};
    return out.substr(first);
}

bool TryParseRoundLabel(std::string_view text, std::string& label) {
    if (ParsePattern(NormalizeOcrRoundText(text), label, false)) return true;
    // Segundo passo, mais conservador que o primeiro: aceita confusões de
    // glifos (S->5, O->0) somente se a linha INTEIRA for o contador. Assim
    // "1-S" vira "1-5", mas um banner como "BOAS-VINDAS A RODADA 2!" não
    // vira um rótulo falso.
    std::string compact;
    std::size_t i = 0;
    std::uint32_t cp = 0;
    while (NextCodePoint(text, i, cp)) {
        if (cp >= '0' && cp <= '9') {
            compact.push_back(static_cast<char>(cp));
        } else if (IsDashCodePoint(cp)) {
            compact.push_back('-');
        } else if (IsSpaceCodePoint(cp)) {
            continue;
        } else if (cp == 'S' || cp == 's') {
            compact.push_back('5');
        } else if (cp == 'O' || cp == 'o') {
            compact.push_back('0');
        } else {
            return false;
        }
    }
    return ParsePattern(compact, label, true);
}

bool ParseExactRoundLabel(std::string_view text, std::string& label) {
    // Texto bruto só pode conter dígitos, espaços e hífen: nada de "ROUND".
    std::string compact;
    std::size_t i = 0;
    std::uint32_t cp = 0;
    while (NextCodePoint(text, i, cp)) {
        if (cp >= '0' && cp <= '9') {
            compact.push_back(static_cast<char>(cp));
        } else if (IsDashCodePoint(cp)) {
            compact.push_back('-');
        } else if (IsSpaceCodePoint(cp)) {
            continue;
        } else {
            return false;
        }
    }
    return ParsePattern(compact, label, true);
}

} // namespace tft
