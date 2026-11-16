#pragma once

#include <string>
#include <vector>

#include "capture/WindowRules.h"

namespace tft {

class TftWindowFinder {
public:
    explicit TftWindowFinder(WindowRules rules = WindowRules{});

    // Carrega regras de arquivo (opcional). Se falhar, mantém regras atuais.
    bool LoadRulesFromFile(const std::string& path, std::string* error = nullptr);

    // Enumera janelas do sistema (Windows) e seleciona a janela do TFT.
    MatchResult Find() const;

    // Enumeração de janelas (implementação dependente de plataforma).
    static std::vector<WindowInfo> EnumerateWindows();

    const WindowRules& Rules() const { return rules_; }

private:
    WindowRules rules_;
};

} // namespace tft
