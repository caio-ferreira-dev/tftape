#include "capture/WindowRules.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace tft {

std::string ToLowerAscii(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

bool ContainsInsensitive(std::string_view haystack, std::string_view needle) {
    auto h = ToLowerAscii(haystack);
    auto n = ToLowerAscii(needle);
    return h.find(n) != std::string::npos;
}

namespace {
bool ExtractStringArray(const std::string& json, const std::string& key, std::vector<std::string>& out, bool& found) {
    found = false;
    std::string pattern = "\"" + key + "\"";
    auto pos = json.find(pattern);
    if (pos == std::string::npos) return true;
    pos = json.find('[', pos + pattern.size());
    if (pos == std::string::npos) return false;
    auto endArr = json.find(']', pos);
    if (endArr == std::string::npos) return false;
    found = true;
    std::vector<std::string> values;
    size_t i = pos + 1;
    while (i < endArr) {
        auto q1 = json.find('"', i);
        if (q1 == std::string::npos || q1 >= endArr) break;
        auto q2 = q1 + 1;
        std::string v;
        while (q2 < endArr) {
            char c = json[q2++];
            if (c == '\\' && q2 < endArr) {
                char n = json[q2++];
                if (n == 'n') v.push_back('\n');
                else if (n == 'r') v.push_back('\r');
                else if (n == 't') v.push_back('\t');
                else v.push_back(n);
            } else if (c == '"') {
                values.push_back(v);
                break;
            } else {
                v.push_back(c);
            }
        }
        i = q2;
    }
    out = std::move(values);
    return true;
}
} // namespace

bool WindowRuleConfig::ParseJson(const std::string& json, WindowRuleConfig& out, std::string* error) {
    WindowRuleConfig cfg = out; // mantém defaults atuais se ausente
    bool found = false;
    if (!ExtractStringArray(json, "title_contains", cfg.title_contains, found)) { if (error) *error = "JSON inválido em title_contains"; return false; }
    if (!ExtractStringArray(json, "process_candidates", cfg.process_candidates, found)) { if (error) *error = "JSON inválido em process_candidates"; return false; }
    if (!ExtractStringArray(json, "process_excludes", cfg.process_excludes, found)) { if (error) *error = "JSON inválido em process_excludes"; return false; }
    out = std::move(cfg);
    return true;
}

bool WindowRuleConfig::LoadFromFile(const std::string& path, WindowRuleConfig& out, std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) { if (error) *error = "Não foi possível abrir arquivo de regras"; return false; }
    std::ostringstream ss; ss << in.rdbuf();
    return ParseJson(ss.str(), out, error);
}

WindowRules::WindowRules(WindowRuleConfig config) : config_(std::move(config)) {}

bool WindowRules::IsExcluded(const WindowInfo& w) const {
    auto p = ToLowerAscii(w.process_name);
    auto t = ToLowerAscii(w.title);
    for (const auto& ex : config_.process_excludes) {
        auto e = ToLowerAscii(ex);
        if (!e.empty() && (p.find(e) != std::string::npos || t.find(e) != std::string::npos)) {
            // evita excluir falso positivo se também for candidato forte de TFT
            // (exclusão só vale se não houver sinal claro de TFT no processo)
            bool tft_signal = false;
            for (const auto& c : config_.process_candidates) {
                if (p.find(ToLowerAscii(c)) != std::string::npos) tft_signal = true;
            }
            if (!tft_signal) return true;
        }
    }
    // exclusão por título típico de LoL (não TFT)
    if (ContainsInsensitive(w.title, "league of legends") && !ContainsInsensitive(w.title, "teamfight")) {
        return true;
    }
    return false;
}

bool WindowRules::IsCandidate(const WindowInfo& w) const {
    if (IsExcluded(w)) return false;
    auto p = ToLowerAscii(w.process_name);
    for (const auto& c : config_.process_candidates) {
        if (!c.empty() && p.find(ToLowerAscii(c)) != std::string::npos) return true;
    }
    for (const auto& tc : config_.title_contains) {
        if (!tc.empty() && ContainsInsensitive(w.title, tc)) {
            // sinal por título: aceito se não for processo explicitamente excluído
            if (!IsExcluded(w)) return true;
        }
    }
    return false;
}

MatchResult WindowRules::SelectTftWindow(const std::vector<WindowInfo>& windows) const {
    std::vector<const WindowInfo*> matches;
    for (const auto& w : windows) {
        if (!w.visible) continue;
        if (IsCandidate(w)) matches.push_back(&w);
    }
    if (matches.empty()) {
        return {MatchStatus::NoCandidate, std::nullopt, "Janela do TFT não encontrada"};
    }
    // preferir candidatos com processo correspondente (mais forte que título)
    std::vector<const WindowInfo*> strong;
    for (auto* w : matches) {
        auto p = ToLowerAscii(w->process_name);
        for (const auto& c : config_.process_candidates) {
            if (!c.empty() && p.find(ToLowerAscii(c)) != std::string::npos) {
                strong.push_back(w);
                break;
            }
        }
    }
    const auto& pool = strong.empty() ? matches : strong;
    if (pool.size() > 1) {
        return {MatchStatus::Ambiguous, std::nullopt, "Múltiplas janelas candidatas ao TFT"};
    }
    const WindowInfo* chosen = pool.front();
    if (chosen->width <= 0 || chosen->height <= 0) {
        return {MatchStatus::NotCapturable, std::nullopt, "Janela do TFT sem dimensões válidas"};
    }
    return {MatchStatus::Matched, *chosen, "OK"};
}

} // namespace tft
