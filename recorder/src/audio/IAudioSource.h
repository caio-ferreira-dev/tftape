#pragma once
#include <cstdint>
#include <vector>

namespace tft {

struct AudioFormat {
    int sample_rate = 48000;
    int channels = 2;
    int bits_per_sample = 16;
};

class IAudioSource {
public:
    virtual ~IAudioSource() = default;
    virtual bool Start() = 0;
    virtual void Stop() = 0;
    // Lê amostras PCM intercaladas (16-bit) disponíveis; retorna bytes lidos.
    virtual size_t Read(uint8_t* dst, size_t max_bytes) = 0;
    virtual AudioFormat GetFormat() const = 0;
    virtual bool IsActive() const = 0;
};

} // namespace tft
