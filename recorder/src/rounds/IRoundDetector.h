#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tft {

/// Retângulo em coordenadas normalizadas (0..1) relativas ao frame capturado.
struct NormalizedRect {
    double x = 0.42;
    double y = 0.02;
    double w = 0.16;
    double h = 0.08;
};

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

    /// Drena trabalho assíncrono pendente antes de ler DrainDetections().
    /// Implementações síncronas não precisam fazer nada.
    virtual void Flush() {}

    /// Devolve (e limpa) os rounds detectados desde a última chamada.
    virtual std::vector<DetectedRound> DrainDetections() = 0;
};

} // namespace tft
