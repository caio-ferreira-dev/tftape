#pragma once

#include <cstdint>
#include <string>

#include "output/RecordingSettings.h"

namespace tft {

struct VideoFrameData {
    // Para integração com D3D11/WGC: ponteiro para textura/handle; em caminho CPU, buffer NV12.
    void* texture = nullptr;      // ID3D11Texture2D* quando disponível
    const uint8_t* data = nullptr; // buffer CPU (NV12) alternativo
    int width = 0;
    int height = 0;
    int stride = 0;
    int64_t timestamp_100ns = 0;
};

struct AudioSamples {
    const uint8_t* data = nullptr;
    uint32_t size_bytes = 0;
    int64_t timestamp_100ns = 0;
};

class MfMp4Writer {
public:
    MfMp4Writer() = default;
    ~MfMp4Writer();

    bool Open(const std::string& path, const VideoProfile& video, int audio_sample_rate = 48000, int audio_channels = 2);
    bool WriteVideoFrame(const VideoFrameData& frame);
    bool WriteAudio(const AudioSamples& samples);
    bool Finalize();
    /// Libera o sink writer sem finalize (usado quando o start falha e não há arquivo válido).
    void Abort();
    void Close();

    const std::string& LastError() const { return last_error_; }

private:
    bool opened_ = false;
    bool finalized_ = false;
    std::string last_error_;
#if defined(_WIN32)
    void* sink_writer_ = nullptr; // IMFSinkWriter*
    void* d3d_device_ = nullptr;  // IMFDXGIDeviceManager* (opcional)
    uint32_t video_stream_index_ = 0;
    uint32_t audio_stream_index_ = 0;
    int64_t start_time_100ns_ = 0;
    int video_width_ = 0;
    int video_height_ = 0;
    int video_fps_ = 30;
    bool has_audio_ = false;
#endif
};

} // namespace tft
