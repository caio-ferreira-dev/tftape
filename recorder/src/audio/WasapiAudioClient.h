#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

namespace tft {

/// Cliente WASAPI compartilhado que sempre entrega PCM 16-bit, estéreo, 48 kHz.
///
/// Modos suportados:
///  - loopback do endpoint de render padrão (fallback quando não há isolamento);
///  - loopback por processo (Windows 10 build 20348+), incluindo ou excluindo a
///    árvore de processos do PID alvo (ex.: isolar o som do TFT);
///  - endpoint de captura padrão (microfone).
///
/// A conversão de formato (float32/44.1 kHz/etc.) é delegada ao engine de áudio
/// via AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM, então o consumidor sempre recebe o
/// formato fixo acima.
class WasapiAudioClient {
public:
    WasapiAudioClient();
    ~WasapiAudioClient();

    WasapiAudioClient(const WasapiAudioClient&) = delete;
    WasapiAudioClient& operator=(const WasapiAudioClient&) = delete;

    bool OpenDefaultRenderLoopback();
    bool OpenProcessLoopback(unsigned long process_id, bool include_process_tree);
    bool OpenCaptureEndpoint();

    /// Existe algum endpoint de reprodução ativo? Usado para recusar uma gravação
    /// que seria silenciosa quando todos os dispositivos de saída estão desativados.
    static bool HasActiveRenderEndpoint();

    /// Lê amostras disponíveis (até `max_bytes`, múltiplo do block align de 4 bytes).
    /// Retorna a quantidade de bytes preenchidos; 0 se nada disponível.
    size_t Read(uint8_t* dst, size_t max_bytes);

    void Close();
    bool IsOpen() const;
    bool IsProcessLoopback() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tft
