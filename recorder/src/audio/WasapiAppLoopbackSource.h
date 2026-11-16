#pragma once
#include "audio/IAudioSource.h"
#include <string>

namespace tft {

// Captura áudio associado ao jogo (padrão). Ideal: loopback por processo (WASAPI process loopback).
// Fallback atual: loopback do endpoint de renderização padrão para garantir captura funcional.
class WasapiAppLoopbackSource : public IAudioSource {
public:
    explicit WasapiAppLoopbackSource(unsigned long process_id = 0);
    ~WasapiAppLoopbackSource() override;
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
    struct Impl;
    Impl* impl_ = nullptr;
#endif
};
} // namespace tft
