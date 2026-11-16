#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace tft {

struct WindowInfo {
    std::string title;
    std::string process_name; // ex.: "TftClient.exe"
    std::string class_name;
    bool visible = true;
    int width = 0;
    int height = 0;
    unsigned long long handle_id = 0; // identificador opaco (HWND em Windows)
};

struct WindowRuleConfig {
    std::vector<std::string> title_contains{ "teamfight tactics", "tft" };
    std::vector<std::string> process_candidates{
        "teamfight tactics.exe", "tft.exe", "tftclient.exe", "tft client.exe"
    };
    std::vector<std::string> process_excludes{
        "leagueclient.exe", "leagueclientux.exe", "league of legends.exe"
    };

    // Carrega regras de um JSON simples com arrays de strings:
    // {"title_contains":[...],"process_candidates":[...],"process_excludes":[...]}
    // Campos ausentes mantêm os valores atuais (padrão). Retorna false se o arquivo não puder ser lido.
    static bool LoadFromFile(const std::string& path, WindowRuleConfig& out, std::string* error = nullptr);
    static bool ParseJson(const std::string& json, WindowRuleConfig& out, std::string* error = nullptr);
};

enum class MatchStatus { Matched, NoCandidate, Ambiguous, NotCapturable };

struct MatchResult {
    MatchStatus status = MatchStatus::NoCandidate;
    const WindowInfo* window = nullptr; // aponta para elemento do vetor de entrada
    std::string reason;
};

class WindowRules {
public:
    explicit WindowRules(WindowRuleConfig config = {});

    // Seleciona a janela do TFT. Retorna Matched com ponteiro válido somente nesse caso.
    MatchResult SelectTftWindow(const std::vector<WindowInfo>& windows) const;

    bool IsExcluded(const WindowInfo& w) const;
    bool IsCandidate(const WindowInfo& w) const;
    const WindowRuleConfig& GetConfig() const { return config_; }

private:
    WindowRuleConfig config_;
};

std::string ToLowerAscii(std::string_view s);
bool ContainsInsensitive(std::string_view haystack, std::string_view needle);

} // namespace tft
