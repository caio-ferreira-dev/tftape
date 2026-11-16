#pragma once
#include "audio/IAudioSource.h"

namespace tft {

class WasapiSystemLoopbackSource : public IAudioSource {
public:
    bool Start() override;
    void Stop() override;
    size_t Read(uint8_t* dst, size_t max_bytes) override;
    AudioFormat GetFormat() const override { return fmt_; }
    bool IsActive() const override { return active_; }
private:
    bool active_ = false;
    AudioFormat fmt_{48000, 2, 16};
};
} // namespace tft
