#pragma once

#include <cstdint>
#include <string>

#include "encode/IEncoder.h"

namespace tft {

struct AudioEncoderConfig {
    int sample_rate = 48000;
    int channels = 2;
    int bits_per_sample = 16;
    int bitrate = 160'000;
    int block_align = 4;
    int avg_bytes_per_second = 192'000;
    std::string input_subtype = "PCM";
    std::string output_subtype = "AAC";
};

// Mapeamento puro (testável sem Windows) de PCM 16-bit -> AAC.
AudioEncoderConfig BuildAudioEncoderConfig(int sample_rate, int channels, int bitrate);

class AudioEncoderMf final : public IEncoder {
public:
    AudioEncoderMf(int sample_rate, int channels, int bitrate);
    ~AudioEncoderMf() override;

    bool Open(IMp4Muxer& muxer, std::string* error) override;
    bool Write(const EncoderSample& sample, std::string* error) override;
    void Close() override;
    bool IsOpen() const override { return open_; }
    uint32_t StreamIndex() const override { return stream_index_; }
    const std::string& LastError() const override { return last_error_; }

private:
    AudioEncoderConfig config_{};
    IMp4Muxer* muxer_ = nullptr;
    uint32_t stream_index_ = 0;
    bool open_ = false;
    std::string last_error_;
};

} // namespace tft
