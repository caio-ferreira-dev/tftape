#pragma once

#include <memory>

#include "audio/IAudioSource.h"

namespace tft {

class WasapiAudioClient;

/// Microfone (endpoint de captura padrão).
class WasapiMicSource : public IAudioSource {
public:
    WasapiMicSource();
    ~WasapiMicSource() override;

    bool Start() override;
    void Stop() override;
    size_t Read(uint8_t* dst, size_t max_bytes) override;
    AudioFormat GetFormat() const override { return fmt_; }
    bool IsActive() const override { return active_; }

private:
    bool active_ = false;
    AudioFormat fmt_{48000, 2, 16};
    std::unique_ptr<WasapiAudioClient> client_;
};

} // namespace tft
