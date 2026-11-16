#pragma once

#include <memory>

#include "audio/IAudioSource.h"

namespace tft {

class WasapiAudioClient;

/// Áudio do jogo (TFT).
///
/// Tenta capturar apenas a árvore de processos do jogo via loopback por processo
/// (WASAPI, Windows 10 build 20348+). Caso o isolamento não esteja disponível,
/// cai para loopback do endpoint de render padrão (pode incluir som do sistema).
class WasapiAppLoopbackSource : public IAudioSource {
public:
    explicit WasapiAppLoopbackSource(unsigned long process_id = 0);
    ~WasapiAppLoopbackSource() override;

    bool Start() override;
    void Stop() override;
    size_t Read(uint8_t* dst, size_t max_bytes) override;
    AudioFormat GetFormat() const override { return fmt_; }
    bool IsActive() const override { return active_; }

    /// true quando o áudio do jogo está isolado por processo (sem som do sistema).
    bool IsProcessIsolated() const { return isolated_; }

private:
    unsigned long pid_ = 0;
    bool active_ = false;
    bool isolated_ = false;
    AudioFormat fmt_{48000, 2, 16};
    std::unique_ptr<WasapiAudioClient> client_;
};

} // namespace tft
