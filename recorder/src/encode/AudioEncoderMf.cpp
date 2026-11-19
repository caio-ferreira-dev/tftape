#include "encode/AudioEncoderMf.h"

#include <algorithm>
#include <cstring>
#include <utility>

#if defined(_WIN32)
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

AudioEncoderConfig BuildAudioEncoderConfig(int sample_rate, int channels, int bitrate) {
    AudioEncoderConfig config;
    config.sample_rate = sample_rate;
    config.channels = channels;
    config.bits_per_sample = 16;
    config.bitrate = bitrate;
    config.block_align = channels * (config.bits_per_sample / 8);
    config.avg_bytes_per_second = config.block_align * sample_rate;
    return config;
}

AudioEncoderMf::AudioEncoderMf(int sample_rate, int channels, int bitrate)
    : config_(BuildAudioEncoderConfig(sample_rate, channels, bitrate)) {}

AudioEncoderMf::~AudioEncoderMf() { Close(); }

bool AudioEncoderMf::Open(IMp4Muxer& muxer, std::string* error) {
    Close();
#if defined(_WIN32)
    ComPtr<IMFMediaType> outAudio;
    MFCreateMediaType(&outAudio);
    outAudio->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    outAudio->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
    outAudio->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, static_cast<UINT32>(config_.sample_rate));
    outAudio->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, static_cast<UINT32>(config_.channels));
    outAudio->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, static_cast<UINT32>(config_.bits_per_sample));
    outAudio->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, static_cast<UINT32>(config_.bitrate / 8));

    ComPtr<IMFMediaType> inAudio;
    MFCreateMediaType(&inAudio);
    inAudio->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    inAudio->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    inAudio->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, static_cast<UINT32>(config_.sample_rate));
    inAudio->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, static_cast<UINT32>(config_.channels));
    inAudio->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, static_cast<UINT32>(config_.bits_per_sample));
    inAudio->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, static_cast<UINT32>(config_.block_align));
    inAudio->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND,
                       static_cast<UINT32>(config_.avg_bytes_per_second));

    uint32_t index = 0;
    if (!muxer.AddStream(outAudio.Get(), index, nullptr)) {
        return Fail(error, "AddStream(audio) falhou");
    }
    if (!muxer.SetInputMediaType(index, inAudio.Get(), nullptr)) {
        return Fail(error, "SetInputMediaType(audio) falhou");
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

bool AudioEncoderMf::Write(const EncoderSample& packet, std::string* error) {
#if defined(_WIN32)
    if (!open_ || !muxer_) return Fail(error, "Encoder de áudio não aberto");
    if (!packet.data || packet.size_bytes == 0) return true;

    ComPtr<IMFMediaBuffer> buffer;
    HRESULT hr = MFCreateMemoryBuffer(static_cast<DWORD>(packet.size_bytes), &buffer);
    if (FAILED(hr)) return Fail(error, "MFCreateMemoryBuffer(audio) falhou");
    BYTE* dst = nullptr; DWORD max_len = 0; DWORD cur_len = 0;
    buffer->Lock(&dst, &max_len, &cur_len);
    std::memcpy(dst, packet.data, (std::min)(static_cast<DWORD>(packet.size_bytes), max_len));
    buffer->Unlock();
    buffer->SetCurrentLength(static_cast<DWORD>(packet.size_bytes));

    ComPtr<IMFSample> mf_sample;
    MFCreateSample(&mf_sample);
    mf_sample->AddBuffer(buffer.Get());
    mf_sample->SetSampleTime(packet.timestamp_100ns);
    LONGLONG duration = static_cast<LONGLONG>(packet.size_bytes) * 10'000'000LL /
                        (static_cast<LONGLONG>(config_.avg_bytes_per_second) > 0
                             ? static_cast<LONGLONG>(config_.avg_bytes_per_second) : 192000LL);
    if (duration <= 0) duration = 100000; // 10ms
    mf_sample->SetSampleDuration(duration);

    if (!muxer_->WriteSample(stream_index_, mf_sample.Get(), nullptr)) {
        return Fail(error, "WriteSample(audio) falhou");
    }
    last_error_.clear();
    return true;
#else
    (void)packet;
    return Fail(error, "Media Foundation disponível apenas no Windows");
#endif
}

void AudioEncoderMf::Close() {
    open_ = false;
    muxer_ = nullptr;
    stream_index_ = 0;
    last_error_.clear();
}

} // namespace tft
