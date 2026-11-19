#pragma once

#include <cstdint>
#include <string>

#include "encode/IEncoder.h"
#include "output/RecordingSettings.h"

namespace tft {

struct VideoEncoderConfig {
    int width = 1280;
    int height = 720;
    int fps = 30;
    int bitrate = 4'000'000;
    int64_t frame_duration_100ns = 333'333;
    std::string input_subtype = "NV12";
    std::string output_subtype = "H264";
};

// Mapeamento puro (testável sem Windows) de VideoProfile -> config do encoder.
VideoEncoderConfig BuildVideoEncoderConfig(const VideoProfile& profile);

class VideoEncoderMf final : public IEncoder {
public:
    explicit VideoEncoderMf(VideoProfile profile);
    ~VideoEncoderMf() override;

    bool Open(IMp4Muxer& muxer, std::string* error) override;
    bool Write(const EncoderSample& sample, std::string* error) override;
    void Close() override;
    bool IsOpen() const override { return open_; }
    uint32_t StreamIndex() const override { return stream_index_; }
    const std::string& LastError() const override { return last_error_; }

private:
    VideoProfile profile_{};
    VideoEncoderConfig config_{};
    IMp4Muxer* muxer_ = nullptr;
    uint32_t stream_index_ = 0;
    bool open_ = false;
    std::string last_error_;
};

} // namespace tft
