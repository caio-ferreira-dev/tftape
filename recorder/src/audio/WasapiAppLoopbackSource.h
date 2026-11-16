#pragma once
#include "audio/IAudioSource.h"
#include <string>

namespace tft {

// Captura loopback do processo do jogo (padrão). Windows: WASAPI (process loopback quando disponível).
class WasapiAppLoopbackSource : public IAudioSource {
public:
    explicit WasapiAppLoopbackSource(unsigned long process_id = 0);
    bool Start() override;
    void Stop() override;
    size_t Read(uint8_t* dst, size_t max_bytes) override;
    AudioFormat GetFormat() const override { return fmt_; }
    bool IsActive() const override { return active_; }

private:
    unsigned long pid_ = 0;
    bool active_ = false;
    AudioFormat fmt_{48000, 2, 16};
#if defined(_WIN32)
    void* audio_client_ = nullptr;
    void* capture_client_ = nullptr;
    void* mm_device_ = nullptr;
#endif
};
} // namespace tft
