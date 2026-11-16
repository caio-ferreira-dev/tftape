#pragma once

#include <memory>

#include "audio/IAudioSource.h"

namespace tft {

class WasapiAudioClient;

/// Som de outras janelas/sistema.
///
/// Quando um PID de jogo é informado, usa loopback por processo no modo
/// EXCLUDE_TARGET_PROCESS_TREE: captura tudo, exceto a árvore de processos do
/// jogo (evita duplicar o áudio quando "som do jogo" também está ativo).
/// Sem PID (ou se o isolamento não estiver disponível), usa loopback do
/// endpoint padrão (pode incluir o som do jogo).
class WasapiSystemLoopbackSource : public IAudioSource {
public:
    explicit WasapiSystemLoopbackSource(unsigned long exclude_process_id = 0);
    ~WasapiSystemLoopbackSource() override;

    bool Start() override;
    void Stop() override;
    size_t Read(uint8_t* dst, size_t max_bytes) override;
    AudioFormat GetFormat() const override { return fmt_; }
    bool IsActive() const override { return active_; }

    /// true quando o áudio do jogo foi excluído via loopback por processo.
    bool IsProcessIsolated() const { return isolated_; }

private:
    unsigned long exclude_pid_ = 0;
    bool active_ = false;
    bool isolated_ = false;
    AudioFormat fmt_{48000, 2, 16};
    std::unique_ptr<WasapiAudioClient> client_;
};

} // namespace tft
