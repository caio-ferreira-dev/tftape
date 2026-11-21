#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "rounds/IRoundDetector.h"

namespace tft {

/// Retângulo em coordenadas normalizadas (0..1) relativas ao frame capturado.
struct NormalizedRect {
    double x = 0.42;
    double y = 0.02;
    double w = 0.16;
    double h = 0.08;
};

struct RoundDetectorConfig {
    NormalizedRect roi{};

    /// Rótulos na ordem em que devem ser atribuídos. Vazio => "Round N".
    std::vector<std::string> labels;

    /// Diferença média de luma (0..255) para considerar que o contador mudou.
    int change_threshold = 14;

    /// Frames consecutivos com o novo estado antes de confirmar o marcador.
    int stability_frames = 4;

    /// Intervalo mínimo entre marcadores, na mesma gravação.
    int cooldown_ms = 2500;

    /// Período inicial ignorado (abertura/introdução da janela do jogo).
    int warmup_ms = 3000;

    /// Emite o primeiro rótulo no início da gravação; as viradas seguintes
    /// recebem os rótulos posteriores (1-1 -> 1-2 -> ...).
    bool emit_initial_round = true;

    std::string source = "hud-counter";
};

/// Detector puro: recebe frames BGRA e emite marcadores quando a assinatura da
/// ROI do contador muda para um novo estado estável. Não depende de Windows,
/// D3D11, OCR ou bibliotecas de imagem — a leitura dos dígitos pode ser
/// acrescentada depois sem alterar o contrato com a sessão de gravação.
class RoundDetector final : public IRoundDetector {
public:
    static constexpr int kSignatureWidth = 16;
    static constexpr int kSignatureHeight = 8;
    static constexpr std::size_t kSignatureSize =
        static_cast<std::size_t>(kSignatureWidth) * kSignatureHeight;
    using SignatureArray = std::array<std::uint8_t, kSignatureSize>;

    explicit RoundDetector(RoundDetectorConfig config = {});

    void Reset() override;
    void ProcessFrame(const FrameView& frame) override;
    std::vector<DetectedRound> DrainDetections() override;

    /// Assinatura em luma da ROI, amostrada em 16x8. Útil em testes e na calibração.
    static SignatureArray Signature(const FrameView& frame, const NormalizedRect& roi);

    /// Diferença média absoluta entre duas assinaturas (0..255).
    static int MeanAbsoluteDifference(const SignatureArray& a, const SignatureArray& b);

private:
    /// Chamado com `mutex_` travado; adiciona um marcador à fila pendente.
    void EmitRound(std::int64_t timestamp_100ns, int diff, int stable_frames);

    RoundDetectorConfig config_;
    std::array<std::uint8_t, kSignatureSize> stable_{};
    std::array<std::uint8_t, kSignatureSize> candidate_{};
    bool initial_pending_ = true;
    bool has_stable_ = false;
    bool has_candidate_ = false;
    int candidate_frames_ = 0;
    std::int64_t change_start_100ns_ = 0;
    std::int64_t last_emission_100ns_ = 0;
    std::size_t next_label_ = 0;
    int detection_count_ = 0;

    mutable std::mutex mutex_;
    std::vector<DetectedRound> pending_;
};

} // namespace tft
