#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace tft {

class OutputPath {
public:
    // Valida que o diretório existe (ou pode ser criado) e é gravável na prática básica.
    static bool EnsureDirectory(const std::filesystem::path& dir, std::string* error = nullptr);

    // Gera um caminho .mp4 único dentro de dir, baseado em prefixo e timestamp.
    static std::filesystem::path MakeUniqueMp4Path(const std::filesystem::path& dir,
                                                   std::string_view prefix = "tft");

    // Sanitiza componente de nome de arquivo.
    static std::string SanitizeFilename(std::string_view name);
};

} // namespace tft
