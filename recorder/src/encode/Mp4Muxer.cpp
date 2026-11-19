#include "encode/Mp4Muxer.h"

#include <utility>

#if defined(_WIN32)
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <windows.h>
#include <wrl/client.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mf.lib")

using Microsoft::WRL::ComPtr;
#endif

namespace tft {
namespace {

bool Fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}

#if defined(_WIN32)
std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring wide(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), wide.data(), size);
    return wide;
}
#endif

} // namespace

Mp4Muxer::~Mp4Muxer() { Close(); }

bool Mp4Muxer::Open(const std::string& path, std::string* error) {
    Close();
#if defined(_WIN32)
    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    if (FAILED(hr)) return Fail(error, "MFStartup falhou");

    ComPtr<IMFSinkWriter> writer;
    hr = MFCreateSinkWriterFromURL(Utf8ToWide(path).c_str(), nullptr, nullptr, &writer);
    if (FAILED(hr)) {
        MFShutdown();
        return Fail(error, "MFCreateSinkWriterFromURL falhou");
    }

    writer->AddRef();
    sink_writer_ = writer.Get();
    opened_ = true;
    finalized_ = false;
    last_error_.clear();
    return true;
#else
    (void)path;
    return Fail(error, "Media Foundation disponível apenas no Windows");
#endif
}

bool Mp4Muxer::AddStream(void* output_media_type, uint32_t& stream_index, std::string* error) {
#if defined(_WIN32)
    if (!opened_ || !sink_writer_) return Fail(error, "Sink writer não aberto");
    auto* writer = reinterpret_cast<IMFSinkWriter*>(sink_writer_);
    auto* out_type = reinterpret_cast<IMFMediaType*>(output_media_type);
    DWORD index = 0;
    HRESULT hr = writer->AddStream(out_type, &index);
    if (FAILED(hr)) return Fail(error, "AddStream falhou");
    stream_index = static_cast<uint32_t>(index);
    return true;
#else
    (void)output_media_type; (void)stream_index;
    return Fail(error, "Media Foundation disponível apenas no Windows");
#endif
}

bool Mp4Muxer::SetInputMediaType(uint32_t stream_index, void* input_media_type, std::string* error) {
#if defined(_WIN32)
    if (!opened_ || !sink_writer_) return Fail(error, "Sink writer não aberto");
    auto* writer = reinterpret_cast<IMFSinkWriter*>(sink_writer_);
    auto* in_type = reinterpret_cast<IMFMediaType*>(input_media_type);
    HRESULT hr = writer->SetInputMediaType(stream_index, in_type, nullptr);
    if (FAILED(hr)) return Fail(error, "SetInputMediaType falhou");
    return true;
#else
    (void)stream_index; (void)input_media_type;
    return Fail(error, "Media Foundation disponível apenas no Windows");
#endif
}

bool Mp4Muxer::WriteSample(uint32_t stream_index, void* sample, std::string* error) {
#if defined(_WIN32)
    if (!opened_ || !sink_writer_) return Fail(error, "Sink writer não aberto");
    auto* writer = reinterpret_cast<IMFSinkWriter*>(sink_writer_);
    auto* mf_sample = reinterpret_cast<IMFSample*>(sample);
    HRESULT hr = writer->WriteSample(stream_index, mf_sample);
    if (FAILED(hr)) return Fail(error, "WriteSample falhou");
    return true;
#else
    (void)stream_index; (void)sample;
    return Fail(error, "Media Foundation disponível apenas no Windows");
#endif
}

bool Mp4Muxer::BeginWriting(std::string* error) {
#if defined(_WIN32)
    if (!opened_ || !sink_writer_) return Fail(error, "Sink writer não aberto");
    auto* writer = reinterpret_cast<IMFSinkWriter*>(sink_writer_);
    HRESULT hr = writer->BeginWriting();
    if (FAILED(hr)) return Fail(error, "BeginWriting falhou");
    return true;
#else
    return Fail(error, "Media Foundation disponível apenas no Windows");
#endif
}

bool Mp4Muxer::Finalize(std::string* error) {
#if defined(_WIN32)
    if (!opened_ || !sink_writer_) return Fail(error, "Sink writer não aberto");
    auto* writer = reinterpret_cast<IMFSinkWriter*>(sink_writer_);
    HRESULT hr = writer->Finalize();
    if (FAILED(hr)) return Fail(error, "Finalize falhou");
    finalized_ = true;
    return true;
#else
    return Fail(error, "Media Foundation disponível apenas no Windows");
#endif
}

void Mp4Muxer::Abort() {
#if defined(_WIN32)
    if (sink_writer_) {
        reinterpret_cast<IMFSinkWriter*>(sink_writer_)->Release();
        sink_writer_ = nullptr;
    }
    if (opened_) MFShutdown();
#endif
    opened_ = false;
    finalized_ = true; // evita que Close() finaliza um writer abortado
    last_error_.clear();
}

void Mp4Muxer::Close() {
#if defined(_WIN32)
    if (sink_writer_) {
        auto* writer = reinterpret_cast<IMFSinkWriter*>(sink_writer_);
        if (opened_ && !finalized_) writer->Finalize();
        writer->Release();
        sink_writer_ = nullptr;
    }
    if (opened_) MFShutdown();
#endif
    opened_ = false;
    finalized_ = false;
    last_error_.clear();
}

} // namespace tft
