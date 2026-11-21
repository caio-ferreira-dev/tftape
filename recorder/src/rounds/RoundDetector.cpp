#include "rounds/RoundDetector.h"

#include <algorithm>
#include <string>
#include <utility>

namespace tft {
namespace {

constexpr std::int64_t kTicksPerMs = 10'000; // 1 tick = 100 ns

int ClampInt(long long value, int low, int high) {
    return static_cast<int>(std::clamp<long long>(value, low, high));
}

} // namespace

RoundDetector::RoundDetector(RoundDetectorConfig config) : config_(std::move(config)) {}

void RoundDetector::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    stable_.fill(0);
    candidate_.fill(0);
    has_stable_ = false;
    has_candidate_ = false;
    candidate_frames_ = 0;
    initial_pending_ = true;
    change_start_100ns_ = 0;
    last_emission_100ns_ = 0;
    next_label_ = 0;
    detection_count_ = 0;
    pending_.clear();
}

void RoundDetector::EmitRound(std::int64_t timestamp_100ns, int diff, int stable_frames) {
    DetectedRound round;
    round.timestamp_100ns = timestamp_100ns;
    round.source = config_.source;
    if (next_label_ < config_.labels.size()) {
        round.label = config_.labels[next_label_];
    } else {
        round.label = "Round " + std::to_string(detection_count_ + 1);
    }
    ++next_label_;
    ++detection_count_;
    round.confidence = std::min(1.0, 0.65 + diff / 100.0 + stable_frames / 100.0);
    last_emission_100ns_ = timestamp_100ns;
    pending_.push_back(std::move(round));
}

RoundDetector::SignatureArray RoundDetector::Signature(
    const FrameView& frame, const NormalizedRect& roi) {
    SignatureArray out{};
    if (!frame.bgra || frame.width <= 0 || frame.height <= 0 || frame.stride <= 0) return out;

    const long long w = frame.width;
    const long long h = frame.height;
    const int x0 = ClampInt(static_cast<long long>(roi.x * w), 0, frame.width - 1);
    const int y0 = ClampInt(static_cast<long long>(roi.y * h), 0, frame.height - 1);
    const int x1 = ClampInt(static_cast<long long>((roi.x + roi.w) * w), x0 + 1, frame.width);
    const int y1 = ClampInt(static_cast<long long>((roi.y + roi.h) * h), y0 + 1, frame.height);
    const int roi_w = x1 - x0;
    const int roi_h = y1 - y0;

    for (int sy = 0; sy < kSignatureHeight; ++sy) {
        const int py = y0 + (sy * roi_h) / kSignatureHeight;
        for (int sx = 0; sx < kSignatureWidth; ++sx) {
            const int px = x0 + (sx * roi_w) / kSignatureWidth;
            const std::uint8_t* p = frame.bgra + static_cast<std::size_t>(py) * frame.stride
                                    + static_cast<std::size_t>(px) * 4;
            // Rec. 601 em inteiros: B*0.114 + G*0.587 + R*0.299 (BGRA na memória).
            const int luma = (29 * p[0] + 150 * p[1] + 77 * p[2]) >> 8;
            out[static_cast<std::size_t>(sy) * kSignatureWidth + sx] =
                static_cast<std::uint8_t>(luma);
        }
    }
    return out;
}

int RoundDetector::MeanAbsoluteDifference(const SignatureArray& a, const SignatureArray& b) {
    long long total = 0;
    for (std::size_t i = 0; i < kSignatureSize; ++i) {
        const int av = a[i], bv = b[i];
        total += av > bv ? av - bv : bv - av;
    }
    return static_cast<int>(total / static_cast<long long>(kSignatureSize));
}

void RoundDetector::ProcessFrame(const FrameView& frame) {
    if (!frame.bgra || frame.width <= 0 || frame.height <= 0 || frame.stride <= 0) return;

    const auto signature = Signature(frame, config_.roi);

    std::lock_guard<std::mutex> lock(mutex_);

    // Aquecimento: a janela ainda pode estar aparecendo/animando. Apenas
    // acompanha a assinatura atual para não marcar a abertura como round.
    if (frame.timestamp_100ns < static_cast<std::int64_t>(config_.warmup_ms) * kTicksPerMs) {
        stable_ = signature;
        has_stable_ = true;
        has_candidate_ = false;
        candidate_frames_ = 0;
        return;
    }

    // Primeiro frame fora do aquecimento: registra o round já em andamento para
    // que a primeira virada receba o rótulo seguinte (1-2, 1-3, ...).
    if (initial_pending_) {
        initial_pending_ = false;
        stable_ = signature;
        has_stable_ = true;
        if (config_.emit_initial_round) EmitRound(0, 0, 1);
        return;
    }

    if (!has_stable_) {
        stable_ = signature;
        has_stable_ = true;
        return;
    }

    const int diff = MeanAbsoluteDifference(signature, stable_);
    if (diff <= config_.change_threshold) {
        // Voltou ao estado conhecido: foi uma animação/passagem, não uma troca.
        has_candidate_ = false;
        candidate_frames_ = 0;
        return;
    }

    if (!has_candidate_ || MeanAbsoluteDifference(signature, candidate_) > config_.change_threshold) {
        candidate_ = signature;
        has_candidate_ = true;
        candidate_frames_ = 1;
        change_start_100ns_ = frame.timestamp_100ns;
    } else {
        ++candidate_frames_;
    }

    if (candidate_frames_ < config_.stability_frames) return;

    const int stable_frames = candidate_frames_;
    if (frame.timestamp_100ns - last_emission_100ns_
        < static_cast<std::int64_t>(config_.cooldown_ms) * kTicksPerMs) {
        // Mantém o candidato: se a mudança durar mais que o cooldown, o marcador
        // ainda deve apontar para o instante original da troca.
        return;
    }

    // Nova assinatura confirmada: passa a ser a referência e gera o marcador.
    stable_ = candidate_;
    has_candidate_ = false;
    candidate_frames_ = 0;
    EmitRound(change_start_100ns_, diff, stable_frames);
}

std::vector<DetectedRound> RoundDetector::DrainDetections() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<DetectedRound> out;
    out.swap(pending_);
    return out;
}

} // namespace tft
