#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "rounds/IRoundDetector.h"

namespace tft {

struct OcrRoundDetectorConfig {
    /// Faixa ampla do topo da janela. O OCR encontra o contador dentro dela,
    /// mesmo com a barra expandida/compacta ou com o indicador deslocado.
    NormalizedRect band{0.15, 0.0, 0.70, 0.18};

    /// Frequência de amostragem. O OCR é rápido (~10-25 ms) e o contador é
    /// pequeno: amostrar mais fino aumenta a chance de acertar a leitura.
    int sample_interval_ms = 100;
    int warmup_ms = 500;

    /// O OCR acerta o contador de forma esporádica. Exigimos N leituras do
    /// mesmo texto dentro de uma janela (não precisam ser consecutivas) e
    /// datamos o marcador na primeira leitura daquele texto.
    int confirmation_samples = 2;
    int confirmation_window_ms = 15'000;
    int cooldown_ms = 1200;

    std::string source = "hud-ocr";
};

/// Detector que lê o contador do HUD com Windows OCR em tempo real.
///
/// A thread de vídeo apenas recorta a faixa superior e copia os pixels para uma
/// fila limitada. Uma worker separada chama o OCR, normaliza o texto e publica
/// os rounds. Assim a captura/encode nunca espera pelo reconhecedor.
class OcrRoundDetector final : public IRoundDetector {
public:
    explicit OcrRoundDetector(OcrRoundDetectorConfig config = {});
    ~OcrRoundDetector() override;

    void Reset() override;
    void ProcessFrame(const FrameView& frame) override;
    void Flush() override;
    std::vector<DetectedRound> DrainDetections() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tft
