#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tft {

/// Uma imagem BGRA pronta para análise (o mesmo buffer lido para o encode).
struct FrameView {
    const std::uint8_t* bgra = nullptr;
    int width = 0;
    int height = 0;
    int stride = 0; // bytes por linha (pode incluir padding)
    std::int64_t timestamp_100ns = 0; // relativo ao início da gravação
};

/// Round detectado no HUD. O timestamp usa o mesmo relógio monotonic da mídia.
struct DetectedRound {
    std::string label;
    std::int64_t timestamp_100ns = 0;
    std::string source;
    double confidence = 0.0;
};

/// Responsabilidade isolada de observar a ROI do contador de round e avisar
/// quando ela muda para um novo estado estável. Implementações puras são
/// testáveis sem Windows/GPU; o adaptador WGC apenas entrega os frames.
class IRoundDetector {
public:
    virtual ~IRoundDetector() = default;

    /// Zera o estado interno no início de cada gravação.
    virtual void Reset() = 0;

    /// Chamado na thread de vídeo, uma vez por frame encaminhado ao encoder.
    virtual void ProcessFrame(const FrameView& frame) = 0;

    /// Devolve (e limpa) os rounds detectados desde a última chamada.
    virtual std::vector<DetectedRound> DrainDetections() = 0;
};

} // namespace tft
