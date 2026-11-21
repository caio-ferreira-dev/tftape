#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "rounds/IRoundDetector.h"

namespace tft {

struct OcrRoundDetectorConfig {
    /// Faixa ampla do topo da janela. O OCR encontra o contador dentro dela
    /// (o texto fica em y ~0%-4% e y ~9%-18%; a faixa estreita sobre a barra
    /// sozinha não contém texto legível). Quando o OCR acerta, o detector
    /// trava um recorte menor ao redor da palavra do contador.
    NormalizedRect band{0.15, 0.0, 0.70, 0.18};

    /// Pré-processamento antes de chamar o OCR: cinza + contraste, e ampliação
    /// inteira quando o recorte do contador já foi localizado.
    int contrast_percent = 140;
    int tight_scale = 3;
    int lock_margin_percent = 30;
    int lock_miss_samples = 12;

    /// Refino do timestamp: procura a mudança visual da faixa logo antes da
    /// primeira leitura confirmada. O rótulo continua vindo só do OCR. A
    /// janela é curta (5 s) para não confundir banners/animações com a virada.
    int transition_lookback_ms = 5'000;
    int transition_min_diff = 20;

    /// Frequência de amostragem. O OCR é rápido (~10-25 ms) e o contador é
    /// pequeno: amostrar mais fino aumenta a chance de acertar a leitura.
    int sample_interval_ms = 100;
    int warmup_ms = 500;

    /// O OCR acerta o contador de forma esporádica: num VOD real, as duas
    /// leituras do 1-2 estiveram 19 s apartadas. Exigimos N leituras do mesmo
    /// texto dentro de uma janela generosa (não precisam ser consecutivas) e
    /// datamos o marcador na primeira leitura daquele texto.
    int confirmation_samples = 2;
    int confirmation_window_ms = 60'000;
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
