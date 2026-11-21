#include "rounds/OcrTransition.h"

#include <cstddef>

namespace tft {
namespace detail {
namespace {

int MeanAbsDiff(const std::vector<std::uint8_t>& a, const std::vector<std::uint8_t>& b) {
    if (a.empty() || a.size() != b.size()) return -1;
    unsigned long long sum = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        sum += a[i] > b[i] ? static_cast<unsigned>(a[i] - b[i])
                           : static_cast<unsigned>(b[i] - a[i]);
    }
    return static_cast<int>(sum / a.size());
}

} // namespace

std::int64_t FindOcrTransitionImpl(const OcrSignatureSample* samples,
                                   std::size_t count,
                                   std::int64_t first_read_100ns,
                                   std::int64_t lookback_100ns,
                                   int min_diff) {
    if (count < 3 || first_read_100ns <= 0 || !samples) return 0;

    std::size_t target = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (samples[i].timestamp_100ns > first_read_100ns) break;
        target = i;
    }
    if (target < 2) return 0;

    const auto& target_signature = samples[target].signature;
    if (target_signature.empty()) return 0;

    const std::int64_t oldest = first_read_100ns - lookback_100ns;
    int best_score = -1;
    std::int64_t best_timestamp = 0;

    for (std::size_t i = 1; i < target; ++i) {
        if (samples[i].timestamp_100ns < oldest) continue;
        const auto& before = samples[i - 1].signature;
        const auto& current = samples[i].signature;
        const auto& after = samples[i + 1].signature;

        const int changed = MeanAbsDiff(before, current);
        const int settled = MeanAbsDiff(current, after);
        const int dist_before = MeanAbsDiff(before, target_signature);
        const int dist_after = MeanAbsDiff(current, target_signature);
        if (changed < min_diff || settled < 0 || dist_before < 0 || dist_after < 0) continue;
        // A imagem depois da virada deve estar mais parecida com a do OCR do
        // que a imagem antes: ignora flash/animação que volta ao estado anterior.
        if (dist_before <= dist_after + 2) continue;

        const int score = changed - settled;
        if (score > best_score) {
            best_score = score;
            best_timestamp = samples[i].timestamp_100ns;
        }
    }
    return best_timestamp;
}

} // namespace detail
} // namespace tft
