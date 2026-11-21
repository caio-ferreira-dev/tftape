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

bool ParsePattern(std::string_view normalized, std::string& label, bool exact) {
    for (std::size_t dash = 0; dash < normalized.size(); ++dash) {
        if (normalized[dash] != '-') continue;

        std::size_t begin = dash;
        while (begin > 0 && (dash - begin) < 2 && IsDigit(normalized[begin - 1])) --begin;
        if (begin == dash) continue;
        if (begin > 0 && IsDigit(normalized[begin - 1])) continue;

        std::size_t end = dash + 1;
        while (end < normalized.size() && (end - dash - 1) < 2 && IsDigit(normalized[end])) ++end;
        if (end == dash + 1) continue;
        if (end < normalized.size() && IsDigit(normalized[end])) continue;

        if (exact && (begin != 0 || end != normalized.size())) continue;

        int stage_number = 0;
        for (std::size_t k = begin; k < dash; ++k) {
            stage_number = stage_number * 10 + ToDigit(normalized[k]);
        }
        int round_number = 0;
        for (std::size_t k = dash + 1; k < end; ++k) {
            round_number = round_number * 10 + ToDigit(normalized[k]);
        }

        if (stage_number < 1 || stage_number > 9) continue;
        if (round_number < 1 || round_number > 9) continue;

        label = std::to_string(stage_number) + "-" + std::to_string(round_number);
        return true;
    }
    return false;
}

} // namespace

std::string NormalizeOcrRoundText(std::string_view text) {
    std::string out;
    bool last_was_dash = false;

    std::size_t i = 0;
    while (i < text.size()) {
        const auto lead = static_cast<unsigned char>(text[i]);
        std::uint32_t cp = 0;
        std::size_t len = 0;

        if (lead < 0x80) {
            cp = lead;
            len = 1;
        } else if ((lead & 0xE0) == 0xC0) {
            len = 2;
            cp = lead & 0x1F;
        } else if ((lead & 0xF0) == 0xE0) {
            len = 3;
            cp = lead & 0x0F;
        } else if ((lead & 0xF8) == 0xF0) {
            len = 4;
            cp = lead & 0x07;
        } else {
            ++i;
            continue;
        }

        if (i + len > text.size()) { ++i; continue; }
        for (std::size_t j = 1; j < len; ++j) {
            const auto next = static_cast<unsigned char>(text[i + j]);
            if ((next & 0xC0) != 0x80) { len = 0; break; }
            cp = (cp << 6) | (next & 0x3F);
        }
        if (len == 0) { ++i; continue; }
        i += len;

        if (cp >= '0' && cp <= '9') {
            out.push_back(static_cast<char>(cp));
            last_was_dash = false;
        } else if (IsDashCodePoint(cp)) {
            if (!out.empty() && !last_was_dash) {
                out.push_back('-');
                last_was_dash = true;
            }
        } else if (cp == 'I' || cp == 'i' || cp == 'l' || cp == 'L') {
            out.push_back('1');
            last_was_dash = false;
        }
    }

    while (!out.empty() && out.back() == '-') out.pop_back();
    return out;
}

bool TryParseRoundLabel(std::string_view text, std::string& label) {
    return ParsePattern(NormalizeOcrRoundText(text), label, false);
}

bool ParseExactRoundLabel(std::string_view text, std::string& label) {
    return ParsePattern(NormalizeOcrRoundText(text), label, true);
}

} // namespace tft
