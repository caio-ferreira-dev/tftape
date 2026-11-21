#pragma once

#include <cstdint>
#include <vector>

namespace tft {

struct OcrSignatureSample {
    std::int64_t timestamp_100ns = 0;
    std::vector<std::uint8_t> signature;
};

/// Encontra o instante em que a imagem mudou logo antes da primeira leitura
/// confirmada do contador. O OCR continua sendo a única fonte do rótulo: esta
/// função só afina o `startMs` usando a mudança visual na faixa.
///
/// Retorna 0 quando não há transição confiante (chamador mantém o timestamp
/// da primeira leitura).
std::int64_t FindOcrTransition(const std::vector<OcrSignatureSample>& samples,
                               std::int64_t first_read_100ns,
                               std::int64_t lookback_100ns,
                               int min_diff);

} // namespace tft
