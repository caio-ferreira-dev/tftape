#include "encode/MfMp4Writer.h"

#include <memory>

#include "encode/AudioEncoderMf.h"
#include "encode/Mp4Muxer.h"
#include "encode/VideoEncoderMf.h"

namespace tft {

MfMp4Writer::~MfMp4Writer() { Close(); }

bool MfMp4Writer::Open(const std::string& path, const VideoProfile& video,
                       int audio_sample_rate, int audio_channels) {
    Close();

    muxer_ = std::make_unique<Mp4Muxer>();
    if (!muxer_->Open(path, &last_error_)) {
        muxer_.reset();
        return false;
    }

    video_encoder_ = std::make_unique<VideoEncoderMf>(video);
    if (!video_encoder_->Open(*muxer_, &last_error_)) {
        Abort();
        return false;
    }

    // AAC é opcional no nível do encoder: se o stream falhar, o MP4 segue só com vídeo.
    audio_encoder_ = std::make_unique<AudioEncoderMf>(audio_sample_rate, audio_channels,
                                                      video.audio_bitrate);
    std::string audio_error;
    has_audio_ = audio_encoder_->Open(*muxer_, &audio_error);
    if (!has_audio_) audio_encoder_->Close();

    if (!muxer_->BeginWriting(&last_error_)) {
        Abort();
        return false;
    }
    last_error_.clear();
    return true;
}

bool MfMp4Writer::WriteVideoFrame(const VideoFrameData& frame) {
    if (!video_encoder_ || !video_encoder_->IsOpen()) {
        last_error_ = "Writer de vídeo não aberto";
        return false;
    }
    EncoderSample packet;
    packet.data = frame.data;
    packet.width = frame.width;
    packet.height = frame.height;
    packet.stride = frame.stride;
    packet.timestamp_100ns = frame.timestamp_100ns;
    if (!video_encoder_->Write(packet, &last_error_)) return false;
    return true;
}

bool MfMp4Writer::WriteAudio(const AudioSamples& samples) {
    if (!has_audio_ || !audio_encoder_ || !audio_encoder_->IsOpen()) return true;
    if (!samples.data || samples.size_bytes == 0) return true;

    EncoderSample packet;
    packet.data = samples.data;
    packet.size_bytes = samples.size_bytes;
    packet.timestamp_100ns = samples.timestamp_100ns;
    if (!audio_encoder_->Write(packet, &last_error_)) return false;
    return true;
}

bool MfMp4Writer::Finalize() {
    if (!muxer_) {
        last_error_ = "Writer não aberto";
        return false;
    }
    if (!muxer_->Finalize(&last_error_)) return false;
    return true;
}

void MfMp4Writer::Abort() {
    if (video_encoder_) video_encoder_->Close();
    if (audio_encoder_) audio_encoder_->Close();
    if (muxer_) muxer_->Abort();
    video_encoder_.reset();
    audio_encoder_.reset();
    muxer_.reset();
    has_audio_ = false;
    // Mantém last_error_: chamadores leem a mensagem após falha no Open.
}

void MfMp4Writer::Close() {
    if (video_encoder_) video_encoder_->Close();
    if (audio_encoder_) audio_encoder_->Close();
    if (muxer_) muxer_->Close();
    video_encoder_.reset();
    audio_encoder_.reset();
    muxer_.reset();
    has_audio_ = false;
    last_error_.clear();
}

} // namespace tft
