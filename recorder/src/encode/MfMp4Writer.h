#pragma once

#include <memory>
#include <string>

#include "encode/IMp4Muxer.h"
#include "encode/IMp4Writer.h"
#include "encode/IEncoder.h"

namespace tft {

// Fachada mantida por compatibilidade: compõe Mp4Muxer + VideoEncoderMf +
// AudioEncoderMf e expõe a API usada pela RecordingSession.
class MfMp4Writer final : public IMp4Writer {
public:
    MfMp4Writer() = default;
    ~MfMp4Writer() override;

    bool Open(const std::string& path, const VideoProfile& video,
              int audio_sample_rate = 48000, int audio_channels = 2) override;
    bool WriteVideoFrame(const VideoFrameData& frame) override;
    bool WriteAudio(const AudioSamples& samples) override;
    bool Finalize() override;
    void Abort() override;
    void Close() override;
    const std::string& LastError() const override { return last_error_; }

private:
    std::unique_ptr<IMp4Muxer> muxer_;
    std::unique_ptr<IEncoder> video_encoder_;
    std::unique_ptr<IEncoder> audio_encoder_;
    bool has_audio_ = false;
    std::string last_error_;
};

} // namespace tft
