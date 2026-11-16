#include "audio/WasapiAudioClient.h"

#include "audio/PcmStreamBuffer.h"

#if defined(_WIN32)
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <propidl.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

#if __has_include(<audioclientactivationparams.h>)
#include <audioclientactivationparams.h>
#define TFT_HAS_PROCESS_LOOPBACK 1
#else
#define TFT_HAS_PROCESS_LOOPBACK 0
#endif

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "mmdevapi.lib")

using Microsoft::WRL::ComPtr;

namespace tft {

namespace {

constexpr int kSampleRate = 48000;
constexpr int kChannels = 2;
constexpr int kBitsPerSample = 16;
constexpr int kBlockAlign = kChannels * kBitsPerSample / 8; // 4 bytes por frame

struct EventHandle {
    HANDLE handle = nullptr;
    ~EventHandle() {
        if (handle) CloseHandle(handle);
    }
};

/// Implementação de IActivateAudioInterfaceCompletionHandler usada para aguardar
/// a ativação assíncrona do cliente de loopback por processo.
class ActivationHandler final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          Microsoft::WRL::FtmBase,
          IActivateAudioInterfaceCompletionHandler> {
public:
    explicit ActivationHandler(std::shared_ptr<EventHandle> done) : done_(std::move(done)) {}

    STDMETHOD(ActivateCompleted)(IActivateAudioInterfaceAsyncOperation* operation) override {
        HRESULT activate_hr = E_UNEXPECTED;
        ComPtr<IUnknown> punk;
        HRESULT hr = operation ? operation->GetActivateResult(&activate_hr, &punk) : E_POINTER;
        if (SUCCEEDED(hr)) hr = activate_hr;
        if (SUCCEEDED(hr)) hr = punk.As(&client_);
        result_ = hr;
        if (done_ && done_->handle) SetEvent(done_->handle);
        return S_OK;
    }

    HRESULT Result() const { return result_; }
    ComPtr<IAudioClient> Client() const { return client_; }

private:
    std::shared_ptr<EventHandle> done_;
    HRESULT result_ = E_UNEXPECTED;
    ComPtr<IAudioClient> client_;
};

void CopyPacket(uint8_t* dst, const BYTE* src, size_t bytes, DWORD flags) {
    if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) || !src) {
        std::memset(dst, 0, bytes);
    } else {
        std::memcpy(dst, src, bytes);
    }
}

} // namespace

struct WasapiAudioClient::Impl {
    ComPtr<IAudioClient> client;
    ComPtr<IAudioCaptureClient> capture;
    std::shared_ptr<EventHandle> event;
    PcmStreamBuffer pending{1u << 20}; // ~5s de áudio 48k/16bit/estéreo
    bool open = false;
    bool process_loopback = false;

    void Close() {
        if (client) client->Stop();
        capture.Reset();
        client.Reset();
        event.reset();
        pending.Clear();
        open = false;
        process_loopback = false;
    }

    bool ActivateDefaultEndpoint(bool capture_endpoint, ComPtr<IAudioClient>& out) {
        ComPtr<IMMDeviceEnumerator> enumerator;
        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                      IID_PPV_ARGS(&enumerator));
        if (FAILED(hr)) return false;
        ComPtr<IMMDevice> device;
        hr = enumerator->GetDefaultAudioEndpoint(capture_endpoint ? eCapture : eRender, eConsole, &device);
        if (FAILED(hr)) return false;
        return SUCCEEDED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &out));
    }

    bool ActivateProcessLoopback(unsigned long process_id, bool include_tree, ComPtr<IAudioClient>& out) {
#if TFT_HAS_PROCESS_LOOPBACK
        AUDIOCLIENT_ACTIVATION_PARAMS params{};
        params.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
        params.ProcessLoopbackParams.TargetProcessId = static_cast<DWORD>(process_id);
        params.ProcessLoopbackParams.ProcessLoopbackMode =
            include_tree ? PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE
                         : PROCESS_LOOPBACK_MODE_EXCLUDE_TARGET_PROCESS_TREE;

        PROPVARIANT prop{};
        prop.vt = VT_BLOB;
        prop.blob.cbSize = sizeof(params);
        prop.blob.pBlobData = reinterpret_cast<BYTE*>(&params);

        auto done = std::make_shared<EventHandle>();
        done->handle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!done->handle) return false;

        ComPtr<ActivationHandler> handler = Microsoft::WRL::Make<ActivationHandler>(done);
        if (!handler) return false;

        ComPtr<IActivateAudioInterfaceAsyncOperation> op;
        HRESULT hr = ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,
                                                 __uuidof(IAudioClient), &prop, handler.Get(), &op);
        if (FAILED(hr)) return false;

        if (WaitForSingleObject(done->handle, 5000) != WAIT_OBJECT_0) return false;
        if (FAILED(handler->Result())) return false;
        out = handler->Client();
        return out != nullptr;
#else
        (void)process_id;
        (void)include_tree;
        (void)out;
        return false;
#endif
    }

    bool Open(bool use_process_loopback, unsigned long process_id, bool include_tree,
              bool capture_endpoint) {
        Close();

        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE && hr != S_FALSE) return false;

        ComPtr<IAudioClient> audio;
        if (use_process_loopback) {
            if (process_id == 0) return false;
            if (!ActivateProcessLoopback(process_id, include_tree, audio)) return false;
        } else if (!ActivateDefaultEndpoint(capture_endpoint, audio)) {
            return false;
        }

        // Formato fixo de saída; AUTOCONVERTPCM delega resample/conversão ao engine.
        WAVEFORMATEX fmt{};
        fmt.wFormatTag = WAVE_FORMAT_PCM;
        fmt.nChannels = kChannels;
        fmt.nSamplesPerSec = kSampleRate;
        fmt.wBitsPerSample = kBitsPerSample;
        fmt.nBlockAlign = kBlockAlign;
        fmt.nAvgBytesPerSec = kSampleRate * kBlockAlign;

        DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                      AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
        if (!capture_endpoint) flags |= AUDCLNT_STREAMFLAGS_LOOPBACK;

        hr = audio->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, 0, 0, &fmt, nullptr);
        if (FAILED(hr)) return false;

        auto ev = std::make_shared<EventHandle>();
        ev->handle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!ev->handle) return false;
        if (FAILED(audio->SetEventHandle(ev->handle))) return false;

        ComPtr<IAudioCaptureClient> cap;
        if (FAILED(audio->GetService(IID_PPV_ARGS(&cap)))) return false;
        if (FAILED(audio->Start())) return false;

        client = audio;
        capture = cap;
        event = ev;
        process_loopback = use_process_loopback;
        open = true;
        return true;
    }
};

WasapiAudioClient::WasapiAudioClient() : impl_(std::make_unique<Impl>()) {}
WasapiAudioClient::~WasapiAudioClient() { if (impl_) impl_->Close(); }

bool WasapiAudioClient::OpenDefaultRenderLoopback() {
    return impl_ && impl_->Open(false, 0, false, false);
}

bool WasapiAudioClient::OpenProcessLoopback(unsigned long process_id, bool include_process_tree) {
    return impl_ && impl_->Open(true, process_id, include_process_tree, false);
}

bool WasapiAudioClient::OpenCaptureEndpoint() {
    return impl_ && impl_->Open(false, 0, false, true);
}

void WasapiAudioClient::Close() {
    if (impl_) impl_->Close();
}

bool WasapiAudioClient::IsOpen() const { return impl_ && impl_->open; }
bool WasapiAudioClient::IsProcessLoopback() const { return impl_ && impl_->process_loopback; }

size_t WasapiAudioClient::Read(uint8_t* dst, size_t max_bytes) {
    if (!impl_ || !impl_->open || !impl_->capture || !dst) return 0;
    max_bytes -= max_bytes % static_cast<size_t>(kBlockAlign);
    if (max_bytes == 0) return 0;

    // Drena sobras de pacotes anteriores antes de ler novos.
    size_t written = impl_->pending.Read(dst, max_bytes);
    while (written + kBlockAlign <= max_bytes) {
        UINT32 packet = 0;
        if (FAILED(impl_->capture->GetNextPacketSize(&packet)) || packet == 0) break;

        BYTE* data = nullptr;
        UINT32 frames = 0;
        DWORD flags = 0;
        if (FAILED(impl_->capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr))) break;

        const size_t bytes = static_cast<size_t>(frames) * static_cast<size_t>(kBlockAlign);
        const size_t space = max_bytes - written;
        if (bytes <= space) {
            CopyPacket(dst + written, data, bytes, flags);
            written += bytes;
        } else {
            CopyPacket(dst + written, data, space, flags);
            const size_t remaining = bytes - space;
            if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) || !data) {
                static const std::vector<uint8_t> kZeros(4096, 0);
                size_t left = remaining;
                while (left > 0) {
                    const size_t n = (std::min)(left, kZeros.size());
                    impl_->pending.Append(kZeros.data(), n);
                    left -= n;
                }
            } else {
                impl_->pending.Append(data + space, remaining);
            }
            written = max_bytes;
        }
        impl_->capture->ReleaseBuffer(frames);
    }
    return written;
}

} // namespace tft

#else // !_WIN32

namespace tft {

struct WasapiAudioClient::Impl {};

WasapiAudioClient::WasapiAudioClient() : impl_(std::make_unique<Impl>()) {}
WasapiAudioClient::~WasapiAudioClient() = default;

bool WasapiAudioClient::OpenDefaultRenderLoopback() { return false; }
bool WasapiAudioClient::OpenProcessLoopback(unsigned long, bool) { return false; }
bool WasapiAudioClient::OpenCaptureEndpoint() { return false; }
size_t WasapiAudioClient::Read(uint8_t*, size_t) { return 0; }
void WasapiAudioClient::Close() {}
bool WasapiAudioClient::IsOpen() const { return false; }
bool WasapiAudioClient::IsProcessLoopback() const { return false; }

} // namespace tft

#endif
