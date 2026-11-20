#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace tft {

/// Marcador de round. A lista começa vazia; a detecção por ROI/HUD será adicionada depois.
struct RoundMarker {
    std::string label;
    std::int64_t startMs = 0;
    std::string source;
    double confidence = 0.0;
};

struct RecordingSidecarData {
    int schemaVersion = 1;
    std::string videoFile;
    std::string createdAtUtc;
    std::int64_t durationMs = 0;
    std::string resolution;
    int fps = 30;
    std::vector<RoundMarker> rounds;
};

/// Caminho do sidecar: <video>.mp4 -> <video>.mp4.tftape.json.
std::filesystem::path SidecarPathFor(const std::filesystem::path& mp4Path);

/// Escreve o sidecar de forma atômica (arquivo temporário + rename).
bool WriteRecordingSidecar(const RecordingSidecarData& data,
                           const std::filesystem::path& mp4Path,
                           std::string& error);

} // namespace tft
