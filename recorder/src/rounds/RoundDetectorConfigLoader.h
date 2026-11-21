#pragma once

#include <string>

#include "rounds/RoundDetector.h"

namespace tft {

/// Configuração padrão usada quando não há arquivo (ROI provisória do topo).
RoundDetectorConfig DefaultRoundDetectorConfig();

/// Lê `tft_round_detector.json`. Arquivo ausente não é erro: mantém os defaults.
/// Configuração inválida devolve false e mantém `out` inalterado.
bool LoadRoundDetectorConfig(const std::string& path,
                             RoundDetectorConfig& out,
                             std::string* error = nullptr);

/// Parser puro usado pelo loader e pelos testes.
bool ParseRoundDetectorConfigJson(const std::string& json,
                                  RoundDetectorConfig& out,
                                  std::string* error = nullptr);

/// Primeiro arquivo existente entre os caminhos relativos usados pelo core.
/// Vazio quando não há configuração (nesse caso os defaults são usados).
std::string FindRoundDetectorConfigPath();

/// Conteúdo do arquivo de exemplo, para serialização/documentação e testes.
std::string RoundDetectorConfigSampleJson();

} // namespace tft
