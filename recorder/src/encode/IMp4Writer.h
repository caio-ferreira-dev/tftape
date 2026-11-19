#pragma once

#include <cstdint>
#include <string>

#include "output/RecordingSettings.h"

namespace tft {

// Formato bruto de vídeo fornecido pela captura (NV12 no caminho atual).
struct VideoFrameData {
    // Para integração com D3D11/WGC: ponteiro para textura/handle; em caminho CPU, buffer NV12.
    void* texture = nullptr;       // ID3D11Texture2D* quando disponível
    const uint8_t* data = nullptr; // buffer CPU (NV12) alternativo
    int width = 0;
    int height = 0;
    int stride = 0;
    int64_t timestamp_100ns = 0;
};

// PCM 16-bit/estéreo vindo do mixer.
struct AudioSamples {
    const uint8_t* data = nullptr;
    uint32_t size_bytes = 0;
    int64_t timestamp_100ns = 0;
};

// Fachada usada pela RecordingSession: esconde a composição muxer + encoders.
class IMp4Writer {
public:
    virtual ~IMp4Writer() = default;
    virtual bool Open(const std::string& path, const VideoProfile& video,
                      int audio_sample_rate = 48000, int audio_channels = 2) = 0;
    virtual bool WriteVideoFrame(const VideoFrameData& frame) = 0;
    virtual bool WriteAudio(const AudioSamples& samples) = 0;
    virtual bool Finalize() = 0;
    /// Libera o sink writer sem finalize (usado quando o start falha e não há arquivo válido).
    virtual void Abort() = 0;
    virtual void Close() = 0;
    virtual const std::string& LastError() const = 0;
};

} // namespace tft
