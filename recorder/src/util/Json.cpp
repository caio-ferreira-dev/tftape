#include "util/Json.h"

#include <cstdint>
#include <utility>

namespace tft::json {
namespace {

bool IsSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool IsDigit(char c) {
    return c >= '0' && c <= '9';
}

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    bool ParseFlat(Object& out, std::string* error) {
        SkipWhitespace();
        if (!Consume('{')) return Fail(error, "objeto esperado no início", pos_);
        SkipWhitespace();
        if (Consume('}')) {
            SkipWhitespace();
            if (pos_ != text_.size()) return Fail(error, "texto após o objeto JSON", pos_);
            return true;
        }

        while (true) {
            SkipWhitespace();
            std::string key;
            if (!ParseString(key)) return Fail(error, "chave de objeto inválida", pos_);
            if (out.find(key) != out.end()) {
                return Fail(error, "chave duplicada: '" + key + "'", pos_);
            }
            SkipWhitespace();
            if (!Consume(':')) return Fail(error, "':' esperado após a chave '" + key + "'", pos_);
            SkipWhitespace();

            Field field;
            if (pos_ < text_.size() && text_[pos_] == '"') {
                if (!ParseString(field.text)) return Fail(error, "string inválida na chave '" + key + "'", pos_);
                field.type = ValueType::String;
            } else if (StartsWith("true") && IsDelimiterAt(pos_ + 4)) {
                field.type = ValueType::Bool;
                field.bool_value = true;
                pos_ += 4;
            } else if (StartsWith("false") && IsDelimiterAt(pos_ + 5)) {
                field.type = ValueType::Bool;
                field.bool_value = false;
                pos_ += 5;
            } else {
                field.type = ValueType::Other;
                if (!SkipValue()) return Fail(error, "valor inválido na chave '" + key + "'", pos_);
            }
            out.emplace(std::move(key), std::move(field));

            SkipWhitespace();
            if (Consume('}')) break;
            if (!Consume(',')) return Fail(error, "',' ou '}' esperado", pos_);
            SkipWhitespace();
            if (Consume('}')) return Fail(error, "vírgula final não permitida", pos_);
        }

        SkipWhitespace();
        if (pos_ != text_.size()) return Fail(error, "texto após o objeto JSON", pos_);
        return true;
    }

private:
    static bool Fail(std::string* error, std::string_view message, std::size_t pos) {
        if (error) {
            *error = "JSON inválido na posição " + std::to_string(pos + 1) + ": " + std::string(message);
        }
        return false;
    }

    void SkipWhitespace() {
        while (pos_ < text_.size() && IsSpace(text_[pos_])) ++pos_;
    }

    bool Consume(char c) {
        if (pos_ < text_.size() && text_[pos_] == c) {
            ++pos_;
            return true;
        }
        return false;
    }

    bool StartsWith(std::string_view prefix) const {
        return pos_ + prefix.size() <= text_.size() &&
               text_.compare(pos_, prefix.size(), prefix) == 0;
    }

    bool IsDelimiterAt(std::size_t p) const {
        if (p >= text_.size()) return true;
        const char c = text_[p];
        return IsSpace(c) || c == ',' || c == '}' || c == ']';
    }

    bool ParseHex4(std::uint32_t& out) {
        if (pos_ + 4 > text_.size()) return false;
        std::uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = text_[pos_++];
            value <<= 4;
            if (c >= '0' && c <= '9') value += static_cast<std::uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') value += static_cast<std::uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') value += static_cast<std::uint32_t>(c - 'A' + 10);
            else return false;
        }
        out = value;
        return true;
    }

    static void AppendUtf8(std::uint32_t cp, std::string& out) {
        if (cp <= 0x7F) {
            out.push_back(static_cast<char>(cp));
        } else if (cp <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    bool ParseString(std::string& out) {
        if (!Consume('"')) return false;
        out.clear();
        while (pos_ < text_.size()) {
            const auto c = static_cast<unsigned char>(text_[pos_++]);
            if (c == '"') return true;
            if (c < 0x20) return false;
            if (c != '\\') {
                out.push_back(static_cast<char>(c));
                continue;
            }
            if (pos_ >= text_.size()) return false;
            const char esc = text_[pos_++];
            switch (esc) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    std::uint32_t cp = 0;
                    if (!ParseHex4(cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (pos_ + 1 >= text_.size() || text_[pos_] != '\\' || text_[pos_ + 1] != 'u') return false;
                        pos_ += 2;
                        std::uint32_t low = 0;
                        if (!ParseHex4(low)) return false;
                        if (low < 0xDC00 || low > 0xDFFF) return false;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        return false; // surrogate baixo sem par
                    }
                    AppendUtf8(cp, out);
                    break;
                }
                default: return false; // escape desconhecido
            }
        }
        return false; // string não terminada
    }

    bool ConsumeLiteral(std::string_view literal) {
        if (!StartsWith(literal) || !IsDelimiterAt(pos_ + literal.size())) return false;
        pos_ += literal.size();
        return true;
    }

    bool ParseNumber() {
        Consume('-');
        if (Consume('0')) {
            if (pos_ < text_.size() && IsDigit(text_[pos_])) return false; // zero à esquerda
        } else {
            if (pos_ >= text_.size() || text_[pos_] < '1' || text_[pos_] > '9') return false;
            while (pos_ < text_.size() && IsDigit(text_[pos_])) ++pos_;
        }
        if (Consume('.')) {
            if (pos_ >= text_.size() || !IsDigit(text_[pos_])) return false;
            while (pos_ < text_.size() && IsDigit(text_[pos_])) ++pos_;
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) ++pos_;
            if (pos_ >= text_.size() || !IsDigit(text_[pos_])) return false;
            while (pos_ < text_.size() && IsDigit(text_[pos_])) ++pos_;
        }
        return IsDelimiterAt(pos_);
    }

    bool SkipValue() {
        if (pos_ >= text_.size()) return false;
        const char c = text_[pos_];
        if (c == '"') {
            std::string ignored;
            return ParseString(ignored);
        }
        if (c == '{') return SkipObject();
        if (c == '[') return SkipArray();
        if (c == 't') return ConsumeLiteral("true");
        if (c == 'f') return ConsumeLiteral("false");
        if (c == 'n') return ConsumeLiteral("null");
        if (c == '-' || IsDigit(c)) return ParseNumber();
        return false;
    }

    bool SkipObject() {
        if (!Consume('{')) return false;
        SkipWhitespace();
        if (Consume('}')) return true;
        while (true) {
            SkipWhitespace();
            std::string ignored;
            if (!ParseString(ignored)) return false;
            SkipWhitespace();
            if (!Consume(':')) return false;
            SkipWhitespace();
            if (!SkipValue()) return false;
            SkipWhitespace();
            if (Consume('}')) return true;
            if (!Consume(',')) return false;
            SkipWhitespace();
            if (Consume('}')) return false; // vírgula final
        }
    }

    bool SkipArray() {
        if (!Consume('[')) return false;
        SkipWhitespace();
        if (Consume(']')) return true;
        while (true) {
            SkipWhitespace();
            if (!SkipValue()) return false;
            SkipWhitespace();
            if (Consume(']')) return true;
            if (!Consume(',')) return false;
            SkipWhitespace();
            if (Consume(']')) return false; // vírgula final
        }
    }

    std::string_view text_;
    std::size_t pos_ = 0;
};

} // namespace

bool ParseFlatObject(std::string_view text, Object& out, std::string* error) {
    out.clear();
    Parser parser(text);
    return parser.ParseFlat(out, error);
}

} // namespace tft::json
