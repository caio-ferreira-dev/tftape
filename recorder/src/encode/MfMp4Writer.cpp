#include "encode/MfMp4Writer.h"

#if defined(_WIN32)
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <codecapi.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <algorithm>
#include <cstring>
#include <vector>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "d3d11.lib")

using Microsoft::WRL::ComPtr;
#endif

namespace tft {

MfMp4Writer::~MfMp4Writer() { Close(); }

#if defined(_WIN32)
namespace {
std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int sz = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(sz, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), sz);
    return w;
}
}

bool MfMp4Writer::Open(const std::string& path, const VideoProfile& video, int audio_sample_rate, int audio_channels) {
    Close();
    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    if (FAILED(hr)) { last_error_ = "MFStartup falhou"; return false; }

    ComPtr<IMFSinkWriter> writer;
    hr = MFCreateSinkWriterFromURL(Utf8ToWide(path).c_str(), nullptr, nullptr, &writer);
    if (FAILED(hr)) { last_error_ = "MFCreateSinkWriterFromURL falhou"; return false; }

    // Stream de vídeo (H.264)
    ComPtr<IMFMediaType> outVideo;
    MFCreateMediaType(&outVideo);
    outVideo->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    outVideo->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    outVideo->SetUINT32(MF_MT_AVG_BITRATE, (UINT32)video.video_bitrate);
    outVideo->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeSize(outVideo.Get(), MF_MT_FRAME_SIZE, video.width, video.height);
    MFSetAttributeRatio(outVideo.Get(), MF_MT_FRAME_RATE, video.fps, 1);
    MFSetAttributeRatio(outVideo.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

    ComPtr<IMFMediaType> inVideo;
    MFCreateMediaType(&inVideo);
    inVideo->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    // NV12 é comum para captura/encoder HW; caminho CPU pode fornecer NV12 ou RGB convertido.
    inVideo->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
    inVideo->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeSize(inVideo.Get(), MF_MT_FRAME_SIZE, video.width, video.height);
    MFSetAttributeRatio(inVideo.Get(), MF_MT_FRAME_RATE, video.fps, 1);
    MFSetAttributeRatio(inVideo.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

    DWORD vIndex = 0;
    hr = writer->AddStream(outVideo.Get(), &vIndex);
    if (FAILED(hr)) { last_error_ = "AddStream(video) falhou"; return false; }
    hr = writer->SetInputMediaType(vIndex, inVideo.Get(), nullptr);
    if (FAILED(hr)) { last_error_ = "SetInputMediaType(video) falhou (NV12)"; return false; }

    // Stream de áudio (AAC) - assume PCM 16-bit de entrada
    DWORD aIndex = 0;
    ComPtr<IMFMediaType> outAudio;
    MFCreateMediaType(&outAudio);
    outAudio->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    outAudio->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
    outAudio->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, (UINT32)audio_sample_rate);
    outAudio->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, (UINT32)audio_channels);
    outAudio->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    outAudio->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, (UINT32)video.audio_bitrate / 8);

    ComPtr<IMFMediaType> inAudio;
    MFCreateMediaType(&inAudio);
    inAudio->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    inAudio->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    inAudio->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, (UINT32)audio_sample_rate);
    inAudio->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, (UINT32)audio_channels);
    inAudio->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    UINT32 blockAlign = (UINT32)(audio_channels * 2);
    inAudio->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, blockAlign);
    inAudio->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, blockAlign * (UINT32)audio_sample_rate);

    hr = writer->AddStream(outAudio.Get(), &aIndex);
    bool audioOk = SUCCEEDED(hr) && SUCCEEDED(writer->SetInputMediaType(aIndex, inAudio.Get(), nullptr));

    hr = writer->BeginWriting();
    if (FAILED(hr)) { last_error_ = "BeginWriting falhou"; return false; }

    writer->AddRef();
    sink_writer_ = writer.Get();
    video_stream_index_ = vIndex;
    audio_stream_index_ = aIndex;
    video_width_ = video.width;
    video_height_ = video.height;
    video_fps_ = video.fps;
    has_audio_ = audioOk;
    start_time_100ns_ = 0;
    opened_ = true;
    last_error_.clear();
    return true;
}

bool MfMp4Writer::WriteVideoFrame(const VideoFrameData& frame) {
    if (!opened_ || !sink_writer_) return false;
    auto* writer = reinterpret_cast<IMFSinkWriter*>(sink_writer_);

    // Caminho de buffer CPU NV12 esperado em frame.data (conversão da textura WGC deve ocorrer antes).
    if (!frame.data || frame.width <= 0 || frame.height <= 0) {
        last_error_ = "Frame de vídeo inválido (buffer NV12 ausente)";
        return false;
    }
    const LONG stride = frame.stride > 0 ? frame.stride : frame.width;
    const DWORD bufferSize = (DWORD)(stride * frame.height * 3 / 2); // NV12
    ComPtr<IMFMediaBuffer> buffer;
    HRESULT hr = MFCreateMemoryBuffer(bufferSize, &buffer);
    if (FAILED(hr)) { last_error_ = "MFCreateMemoryBuffer(video) falhou"; return false; }
    BYTE* dst = nullptr; DWORD maxLen = 0, curLen = 0;
    buffer->Lock(&dst, &maxLen, &curLen);
    memcpy(dst, frame.data, (std::min)(bufferSize, maxLen));
    buffer->Unlock();
    buffer->SetCurrentLength(bufferSize);

    ComPtr<IMFSample> sample;
    MFCreateSample(&sample);
    sample->AddBuffer(buffer.Get());
    sample->SetSampleTime(frame.timestamp_100ns);
    const LONGLONG duration = 10'000'000LL / (video_fps_ > 0 ? video_fps_ : 30);
    sample->SetSampleDuration(duration);
    hr = writer->WriteSample(video_stream_index_, sample.Get());
    if (FAILED(hr)) { last_error_ = "WriteSample(video) falhou"; return false; }
    return true;
}

bool MfMp4Writer::WriteAudio(const AudioSamples& samples) {
    if (!opened_ || !sink_writer_ || !has_audio_) return true; // sem áudio é aceitável
    if (!samples.data || samples.size_bytes == 0) return true;
    auto* writer = reinterpret_cast<IMFSinkWriter*>(sink_writer_);
    ComPtr<IMFMediaBuffer> buffer;
    HRESULT hr = MFCreateMemoryBuffer(samples.size_bytes, &buffer);
    if (FAILED(hr)) { last_error_ = "MFCreateMemoryBuffer(audio) falhou"; return false; }
    BYTE* dst = nullptr; DWORD maxLen = 0, curLen = 0;
    buffer->Lock(&dst, &maxLen, &curLen);
    memcpy(dst, samples.data, (std::min)((DWORD)samples.size_bytes, maxLen));
    buffer->Unlock();
    buffer->SetCurrentLength(samples.size_bytes);
    ComPtr<IMFSample> sample;
    MFCreateSample(&sample);
    sample->AddBuffer(buffer.Get());
    sample->SetSampleTime(samples.timestamp_100ns);
    // duração baseada em PCM 16-bit estéreo 48kHz (192000 bytes/s)
    LONGLONG duration = (LONGLONG)samples.size_bytes * 10'000'000LL / (48000LL * 2 * 2);
    if (duration <= 0) duration = 100000; // 10ms
    sample->SetSampleDuration(duration);
    hr = writer->WriteSample(audio_stream_index_, sample.Get());
    if (FAILED(hr)) { last_error_ = "WriteSample(audio) falhou"; return false; }
    return true;
}

bool MfMp4Writer::Finalize() {
    if (!opened_ || !sink_writer_) return false;
    auto* writer = reinterpret_cast<IMFSinkWriter*>(sink_writer_);
    HRESULT hr = writer->Finalize();
    if (FAILED(hr)) { last_error_ = "Finalize falhou"; return false; }
    finalized_ = true;
    return true;
}

void MfMp4Writer::Abort() {
    if (sink_writer_) {
        reinterpret_cast<IMFSinkWriter*>(sink_writer_)->Release();
        sink_writer_ = nullptr;
    }
    if (opened_) MFShutdown();
    opened_ = false;
    finalized_ = true; // evita que Close() tente finalizar um writer abortado
    has_audio_ = false;
}

void MfMp4Writer::Close() {
    if (sink_writer_) {
        auto* writer = reinterpret_cast<IMFSinkWriter*>(sink_writer_);
        // Finalize() e Close() podem ser chamados em sequência; finalizar só uma vez.
        if (opened_ && !finalized_) Finalize();
        writer->Release();
        sink_writer_ = nullptr;
    }
    if (opened_) {
        MFShutdown();
    }
    opened_ = false;
    finalized_ = false;
    has_audio_ = false;
}
#else
bool MfMp4Writer::Open(const std::string&, const VideoProfile&, int, int) { last_error_ = "Media Foundation disponível apenas no Windows"; return false; }
bool MfMp4Writer::WriteVideoFrame(const VideoFrameData&) { return false; }
bool MfMp4Writer::WriteAudio(const AudioSamples&) { return false; }
bool MfMp4Writer::Finalize() { return false; }
void MfMp4Writer::Abort() {}
void MfMp4Writer::Close() {}
#endif

} // namespace tft
