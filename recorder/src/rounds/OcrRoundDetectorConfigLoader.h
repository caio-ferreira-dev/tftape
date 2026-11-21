#pragma once

#include <string>

#include "rounds/OcrRoundDetector.h"

namespace tft {

/// Configuração padrão: faixa ampla no topo, cobrindo a barra expandida e a
/// compacta sem depender da posição exata do contador.
OcrRoundDetectorConfig DefaultOcrRoundDetectorConfig();

/// Lê `tft_round_detector.json`. Arquivo ausente não é erro; configuração
/// inválida devolve false e mantém `out` inalterado.
bool LoadOcrRoundDetectorConfig(const std::string& path,
                                OcrRoundDetectorConfig& out,
                                std::string* error = nullptr);

/// Parser puro usado pelo loader e pelos testes.
bool ParseOcrRoundDetectorConfigJson(const std::string& json,
                                     OcrRoundDetectorConfig& out,
                                     std::string* error = nullptr);

/// Primeiro arquivo existente entre os caminhos relativos usados pelo core.
std::string FindOcrRoundDetectorConfigPath();

/// Conteúdo do arquivo de exemplo, para documentação e testes.
std::string OcrRoundDetectorConfigSampleJson();

} // namespace tft
