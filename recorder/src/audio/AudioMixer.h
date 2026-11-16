#pragma once
#include <memory>
#include <vector>

#include "audio/IAudioSource.h"

namespace tft {

class AudioMixer {
public:
    void SetGameSource(std::shared_ptr<IAudioSource> src) { game_ = std::move(src); }
    void SetSystemSource(std::shared_ptr<IAudioSource> src) { system_ = std::move(src); }
    void SetMicSource(std::shared_ptr<IAudioSource> src) { mic_ = std::move(src); }

    bool Start();
    void Stop();
    // Produz mix PCM 16-bit estéreo 48k no buffer (preenchido com silêncio se necessário).
    size_t ReadMixed(uint8_t* dst, size_t max_bytes);

    AudioFormat GetFormat() const { return {48000, 2, 16}; }

private:
    void MixInto(int16_t* dst, const int16_t* src, size_t samples);

    std::shared_ptr<IAudioSource> game_;
    std::shared_ptr<IAudioSource> system_;
    std::shared_ptr<IAudioSource> mic_;
    std::vector<uint8_t> scratch_;
};

} // namespace tft
