#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace tft {

class OutputPath {
public:
    // Valida a sintaxe do caminho de saída de forma independente de plataforma:
    // absoluto, sem '.'/'..', sem caracteres inválidos/nomes reservados do Windows
    // e sem componentes terminados em '.' ou espaço.
    static bool ValidateDirectorySyntax(std::string_view dir, std::string* error = nullptr);

    // Valida a sintaxe, garante que o diretório existe (ou pode ser criado) e
    // verifica permissão de escrita antes de iniciar a gravação.
    static bool EnsureDirectory(const std::filesystem::path& dir, std::string* error = nullptr);

    // Gera um caminho .mp4 único dentro de dir, baseado em prefixo e timestamp.
    static std::filesystem::path MakeUniqueMp4Path(const std::filesystem::path& dir,
                                                   std::string_view prefix = "tft");

    // Sanitiza componente de nome de arquivo.
    static std::string SanitizeFilename(std::string_view name);
};

} // namespace tft
