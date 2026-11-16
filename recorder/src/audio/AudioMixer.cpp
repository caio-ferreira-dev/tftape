#include "audio/AudioMixer.h"

#include <algorithm>
#include <cstring>

namespace tft {

bool AudioMixer::Start() {
    bool ok = true;
    if (game_) ok = game_->Start() && ok;
    if (system_) ok = system_->Start() && ok;
    if (mic_) ok = mic_->Start() && ok;
    return ok;
}

void AudioMixer::Stop() {
    if (game_) game_->Stop();
    if (system_) system_->Stop();
    if (mic_) mic_->Stop();
}

void AudioMixer::MixInto(int16_t* dst, const int16_t* src, size_t samples) {
    for (size_t i = 0; i < samples; ++i) {
        int v = static_cast<int>(dst[i]) + static_cast<int>(src[i]);
        v = std::max(-32768, std::min(32767, v));
        dst[i] = static_cast<int16_t>(v);
    }
}

size_t AudioMixer::ReadMixed(uint8_t* dst, size_t max_bytes) {
    if (max_bytes == 0) return 0;
    std::memset(dst, 0, max_bytes);
    scratch_.resize(max_bytes);
    auto* out = reinterpret_cast<int16_t*>(dst);
    size_t samples = max_bytes / sizeof(int16_t);

    auto readAndMix = [&](const std::shared_ptr<IAudioSource>& src) {
        if (!src || !src->IsActive()) return;
        std::memset(scratch_.data(), 0, max_bytes);
        size_t got = src->Read(scratch_.data(), max_bytes);
        if (got == 0) return;
        MixInto(out, reinterpret_cast<const int16_t*>(scratch_.data()), std::min(samples, got / sizeof(int16_t)));
    };

    readAndMix(game_);
    readAndMix(system_);
    readAndMix(mic_);
    return max_bytes;
}

} // namespace tft
