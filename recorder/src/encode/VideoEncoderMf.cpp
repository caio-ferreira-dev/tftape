#include "encode/VideoEncoderMf.h"

#include <algorithm>
#include <cstring>
#include <utility>

#if defined(_WIN32)
#include <d3d11.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <windows.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
#endif

namespace tft {
namespace {

bool Fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}

} // namespace

VideoEncoderConfig BuildVideoEncoderConfig(const VideoProfile& profile) {
    VideoEncoderConfig config;
    config.width = profile.width;
    config.height = profile.height;
    config.fps = profile.fps > 0 ? profile.fps : 30;
    config.bitrate = profile.video_bitrate;
    config.frame_duration_100ns = 10'000'000LL / config.fps;
    return config;
}

VideoEncoderMf::VideoEncoderMf(VideoProfile profile)
    : profile_(profile), config_(BuildVideoEncoderConfig(profile)) {}

VideoEncoderMf::~VideoEncoderMf() { Close(); }

bool VideoEncoderMf::Open(IMp4Muxer& muxer, std::string* error) {
    Close();
#if defined(_WIN32)
    ComPtr<IMFMediaType> outVideo;
    MFCreateMediaType(&outVideo);
    outVideo->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    outVideo->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    outVideo->SetUINT32(MF_MT_AVG_BITRATE, static_cast<UINT32>(config_.bitrate));
    outVideo->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeSize(outVideo.Get(), MF_MT_FRAME_SIZE, config_.width, config_.height);
    MFSetAttributeRatio(outVideo.Get(), MF_MT_FRAME_RATE, config_.fps, 1);
    MFSetAttributeRatio(outVideo.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

    ComPtr<IMFMediaType> inVideo;
    MFCreateMediaType(&inVideo);
    inVideo->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    inVideo->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
    inVideo->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeSize(inVideo.Get(), MF_MT_FRAME_SIZE, config_.width, config_.height);
    MFSetAttributeRatio(inVideo.Get(), MF_MT_FRAME_RATE, config_.fps, 1);
    MFSetAttributeRatio(inVideo.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

    uint32_t index = 0;
    if (!muxer.AddStream(outVideo.Get(), index, nullptr)) {
        return Fail(error, "AddStream(video) falhou");
    }
    if (!muxer.SetInputMediaType(index, inVideo.Get(), nullptr)) {
        return Fail(error, "SetInputMediaType(video) falhou (NV12)");
    }
    stream_index_ = index;
    muxer_ = &muxer;
    open_ = true;
    last_error_.clear();
    return true;
#else
    (void)muxer;
    return Fail(error, "Media Foundation disponível apenas no Windows");
#endif
}

bool VideoEncoderMf::Write(const EncoderSample& packet, std::string* error) {
#if defined(_WIN32)
    if (!open_ || !muxer_) return Fail(error, "Encoder de vídeo não aberto");
    if (!packet.data || packet.width <= 0 || packet.height <= 0) {
        return Fail(error, "Frame de vídeo inválido (buffer NV12 ausente)");
    }
    const LONG stride = packet.stride > 0 ? packet.stride : packet.width;
    const DWORD buffer_size = static_cast<DWORD>(stride * packet.height * 3 / 2);

    ComPtr<IMFMediaBuffer> buffer;
    HRESULT hr = MFCreateMemoryBuffer(buffer_size, &buffer);
    if (FAILED(hr)) return Fail(error, "MFCreateMemoryBuffer(video) falhou");
    BYTE* dst = nullptr; DWORD max_len = 0; DWORD cur_len = 0;
    buffer->Lock(&dst, &max_len, &cur_len);
    std::memcpy(dst, packet.data, (std::min)(buffer_size, max_len));
    buffer->Unlock();
    buffer->SetCurrentLength(buffer_size);

    ComPtr<IMFSample> mf_sample;
    MFCreateSample(&mf_sample);
    mf_sample->AddBuffer(buffer.Get());
    mf_sample->SetSampleTime(packet.timestamp_100ns);
    mf_sample->SetSampleDuration(config_.frame_duration_100ns);

    if (!muxer_->WriteSample(stream_index_, mf_sample.Get(), nullptr)) {
        return Fail(error, "WriteSample(video) falhou");
    }
    last_error_.clear();
    return true;
#else
    (void)packet;
    return Fail(error, "Media Foundation disponível apenas no Windows");
#endif
}

void VideoEncoderMf::Close() {
    open_ = false;
    muxer_ = nullptr;
    stream_index_ = 0;
    last_error_.clear();
}

} // namespace tft
