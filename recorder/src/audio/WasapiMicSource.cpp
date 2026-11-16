#include "audio/WasapiMicSource.h"

#if defined(_WIN32)
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <wrl/client.h>
#include <cstring>
#include <algorithm>

using Microsoft::WRL::ComPtr;

namespace tft {

struct WasapiMicSource::Impl {
    ComPtr<IMMDevice> device;
    ComPtr<IAudioClient> client;
    ComPtr<IAudioCaptureClient> capture;
    WAVEFORMATEX* mixFmt = nullptr;
    HANDLE event = nullptr;
    UINT32 frameBytes = 0;
};

WasapiMicSource::~WasapiMicSource() { Stop(); }

bool WasapiMicSource::Start() {
    Stop();
    impl_ = new Impl();
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;
    ComPtr<IMMDeviceEnumerator> enumerator;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
    if (FAILED(hr)) return false;
    hr = enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &impl_->device);
    if (FAILED(hr)) return false;
    hr = impl_->device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &impl_->client);
    if (FAILED(hr)) return false;
    hr = impl_->client->GetMixFormat(&impl_->mixFmt);
    if (FAILED(hr) || !impl_->mixFmt) return false;

    REFERENCE_TIME bufferDur = 10 * 10000;
    hr = impl_->client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                   bufferDur, 0, impl_->mixFmt, nullptr);
    if (FAILED(hr)) {
        hr = impl_->client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, bufferDur, 0, impl_->mixFmt, nullptr);
        if (FAILED(hr)) return false;
    } else {
        impl_->event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        impl_->client->SetEventHandle(impl_->event);
    }
    hr = impl_->client->GetService(IID_PPV_ARGS(&impl_->capture));
    if (FAILED(hr)) return false;
    impl_->frameBytes = impl_->mixFmt->nBlockAlign;
    hr = impl_->client->Start();
    if (FAILED(hr)) return false;
    active_ = true;
    return true;
}

void WasapiMicSource::Stop() {
    if (!impl_) { active_ = false; return; }
    if (impl_->client) impl_->client->Stop();
    if (impl_->event) { CloseHandle(impl_->event); impl_->event = nullptr; }
    if (impl_->mixFmt) { CoTaskMemFree(impl_->mixFmt); impl_->mixFmt = nullptr; }
    impl_->capture.Reset(); impl_->client.Reset(); impl_->device.Reset();
    delete impl_; impl_ = nullptr;
    active_ = false;
}

size_t WasapiMicSource::Read(uint8_t* dst, size_t max_bytes) {
    if (!active_ || !impl_ || !impl_->capture) return 0;
    size_t written = 0;
    while (written < max_bytes) {
        UINT32 packet = 0;
        if (FAILED(impl_->capture->GetNextPacketSize(&packet)) || packet == 0) break;
        BYTE* data = nullptr; UINT32 frames = 0; DWORD flags = 0;
        if (FAILED(impl_->capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr))) break;
        bool isFloat = (impl_->mixFmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
                       (impl_->mixFmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE && impl_->mixFmt->wBitsPerSample == 32);
        bool isPcm16 = (impl_->mixFmt->wFormatTag == WAVE_FORMAT_PCM && impl_->mixFmt->wBitsPerSample == 16);
        if (isPcm16 && impl_->mixFmt->nSamplesPerSec == 48000 && impl_->mixFmt->nChannels == 2) {
            size_t bytes = (size_t)frames * impl_->frameBytes;
            size_t toCopy = (std::min)(bytes, max_bytes - written);
            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) memcpy(dst + written, data, toCopy);
            else memset(dst + written, 0, toCopy);
            written += toCopy;
        } else if (isFloat && impl_->mixFmt->nSamplesPerSec == 48000) {
            int srcCh = impl_->mixFmt->nChannels;
            const float* fsrc = reinterpret_cast<const float*>(data);
            size_t samplesOut = (std::min)((size_t)frames, (max_bytes - written) / (2 * sizeof(int16_t)));
            int16_t* out = reinterpret_cast<int16_t*>(dst + written);
            for (size_t i = 0; i < samplesOut; ++i) {
                float l = 0, r = 0;
                if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                    if (srcCh >= 2) { l = fsrc[i*srcCh + 0]; r = fsrc[i*srcCh + 1]; }
                    else if (srcCh == 1) { l = r = fsrc[i]; }
                }
                auto conv = [](float v){ v = (std::max)(-1.0f, (std::min)(1.0f, v)); return (int16_t)(v * 32767.0f); };
                out[i*2 + 0] = conv(l); out[i*2 + 1] = conv(r);
            }
            written += samplesOut * 2 * sizeof(int16_t);
        } else {
            size_t toSilence = (std::min)((size_t)frames * 2 * sizeof(int16_t), max_bytes - written);
            memset(dst + written, 0, toSilence);
            written += toSilence;
        }
        impl_->capture->ReleaseBuffer(frames);
        if (written >= max_bytes) break;
    }
    return written;
}

} // namespace tft
#else
namespace tft {
WasapiMicSource::~WasapiMicSource() {}
bool WasapiMicSource::Start() { active_ = false; return false; }
void WasapiMicSource::Stop() { active_ = false; }
size_t WasapiMicSource::Read(uint8_t*, size_t) { return 0; }
} // namespace tft
#endif
