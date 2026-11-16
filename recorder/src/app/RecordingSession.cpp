#include "app/RecordingSession.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <vector>

#include "audio/WasapiAppLoopbackSource.h"
#include "audio/WasapiMicSource.h"
#include "audio/WasapiSystemLoopbackSource.h"
#include "output/OutputPath.h"

#if defined(_WIN32)
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <mfapi.h>
using Microsoft::WRL::ComPtr;
#endif

namespace tft {

RecordingSession::~RecordingSession() { std::string e; Stop(e); }

#if defined(_WIN32)
namespace {
int64_t Now100ns() {
    FILETIME ft; GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli; uli.LowPart = ft.dwLowDateTime; uli.HighPart = ft.dwHighDateTime;
    return static_cast<int64_t>(uli.QuadPart);
}

// Conversão simples BGRA -> NV12 (CPU) com escala nearest-neighbor para resolução alvo.
void BgraToNv12(const uint8_t* bgra, int srcW, int srcH, int srcStride,
                std::vector<uint8_t>& nv12, int dstW, int dstH) {
    nv12.assign(dstW * dstH * 3 / 2, 0);
    uint8_t* yPlane = nv12.data();
    uint8_t* uvPlane = nv12.data() + dstW * dstH;
    for (int y = 0; y < dstH; ++y) {
        int sy = (y * srcH) / dstH;
        for (int x = 0; x < dstW; ++x) {
            int sx = (x * srcW) / dstW;
            const uint8_t* p = bgra + sy * srcStride + sx * 4;
            int b = p[0], g = p[1], r = p[2];
            int yy = (77 * r + 150 * g + 29 * b + 128) >> 8;
            yPlane[y * dstW + x] = static_cast<uint8_t>(std::clamp(yy, 0, 255));
            if ((y % 2 == 0) && (x % 2 == 0)) {
                int uu = ((-43 * r - 85 * g + 128 * b + 128) >> 8) + 128;
                int vv = ((128 * r - 107 * g - 21 * b + 128) >> 8) + 128;
                size_t uvIndex = (y / 2) * dstW + (x & ~1);
                uvPlane[uvIndex + 0] = static_cast<uint8_t>(std::clamp(uu, 0, 255));
                uvPlane[uvIndex + 1] = static_cast<uint8_t>(std::clamp(vv, 0, 255));
            }
        }
    }
}

bool ReadbackTextureBGRA(ID3D11Device* dev, ID3D11DeviceContext* ctx, ID3D11Texture2D* tex,
                         std::vector<uint8_t>& out, int& w, int& h, int& stride) {
    if (!dev || !ctx || !tex) return false;
    D3D11_TEXTURE2D_DESC desc{}; tex->GetDesc(&desc);
    w = (int)desc.Width; h = (int)desc.Height;
    D3D11_TEXTURE2D_DESC sd = desc;
    sd.Usage = D3D11_USAGE_STAGING;
    sd.BindFlags = 0; sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ; sd.MiscFlags = 0;
    ComPtr<ID3D11Texture2D> staging;
    if (FAILED(dev->CreateTexture2D(&sd, nullptr, &staging))) return false;
    ctx->CopyResource(staging.Get(), tex);
    D3D11_MAPPED_SUBRESOURCE map{};
    if (FAILED(ctx->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &map))) return false;
    stride = (int)map.RowPitch;
    out.resize(stride * h);
    memcpy(out.data(), map.pData, out.size());
    ctx->Unmap(staging.Get(), 0);
    return true;
}
} // namespace

bool RecordingSession::Start(const RecordingSettings& settings, void* window_handle, unsigned long process_id, std::string& error) {
    if (running_) { error = "Sessão já em execução"; return false; }
    settings_ = settings;
    stop_requested_ = false;
    std::string pathErr;
    if (!OutputPath::EnsureDirectory(settings_.output_dir, &pathErr)) { error = pathErr; return false; }
    output_path_ = OutputPath::MakeUniqueMp4Path(settings_.output_dir).string();

    if (!capture_.Start(window_handle)) { error = "Falha ao iniciar captura WGC da janela do TFT"; return false; }

    auto profile = settings_.ToProfile();
    if (!writer_.Open(output_path_, profile)) { error = writer_.LastError(); capture_.Stop(); return false; }

    // Áudio: som do jogo (padrão) + opções
    if (settings_.audio.game_audio) { game_src_ = std::make_shared<WasapiAppLoopbackSource>(process_id); mixer_.SetGameSource(game_src_); }
    if (settings_.audio.system_audio) { sys_src_ = std::make_shared<WasapiSystemLoopbackSource>(); mixer_.SetSystemSource(sys_src_); }
    if (settings_.audio.microphone) { mic_src_ = std::make_shared<WasapiMicSource>(); mixer_.SetMicSource(mic_src_); }
    mixer_.Start();

    start_time_100ns_ = Now100ns();
    last_video_100ns_ = 0;
    running_ = true;
    video_thread_ = std::thread([this]() { RunVideoLoop(); });
    audio_thread_ = std::thread([this]() { RunAudioLoop(); });
    return true;
}

bool RecordingSession::Stop(std::string& error) {
    if (!running_ && !video_thread_.joinable() && !audio_thread_.joinable()) return true;
    stop_requested_ = true;
    running_ = false;
    if (audio_thread_.joinable()) audio_thread_.join();
    if (video_thread_.joinable()) video_thread_.join();
    mixer_.Stop();
    capture_.Stop();
    if (!writer_.Finalize()) { error = writer_.LastError(); writer_.Close(); return false; }
    writer_.Close();
    return true;
}

void RecordingSession::RunVideoLoop() {
    try {
        auto profile = settings_.ToProfile();
        const int64_t frameDur = 10'000'000LL / (profile.fps > 0 ? profile.fps : 30);
        std::vector<uint8_t> bgra, nv12;

        while (!stop_requested_) {
            int64_t now = Now100ns() - start_time_100ns_;
            if (now - last_video_100ns_ >= frameDur) {
                last_video_100ns_ += frameDur;
                if (last_video_100ns_ < now - frameDur) last_video_100ns_ = now; // evita acúmulo grande
                WriteVideoTick(last_video_100ns_, bgra, nv12);
            } else {
                Sleep(1);
            }
        }
    } catch (...) {
        // evita término silencioso da thread
    }
}

void RecordingSession::RunAudioLoop() {
    try {
        const int64_t audioChunk = 10'000'000LL / 100; // 10ms
        std::vector<uint8_t> audioBuf(48000 * 2 * 2 / 100);
        int64_t last_audio = 0;
        while (!stop_requested_) {
            int64_t now = Now100ns() - start_time_100ns_;
            if (now - last_audio >= audioChunk) {
                last_audio += audioChunk;
                // limita catch-up para evitar rajadas (ruído/estalos) sob carga
                if (last_audio < now - audioChunk * 2) last_audio = now - audioChunk;
                WriteAudioTick(last_audio, audioBuf);
            } else {
                Sleep(1);
            }
        }
    } catch (...) {
        // evita término silencioso da thread
    }
}

bool RecordingSession::WriteVideoTick(int64_t now_100ns, std::vector<uint8_t>& bgra, std::vector<uint8_t>& nv12) {
    WgcFrame wf{};
    if (!capture_.AcquireTexture(wf) || !wf.texture) return false;
    auto* dev = reinterpret_cast<ID3D11Device*>(capture_.GetD3DDevice());
    if (!dev) return false;
    ComPtr<ID3D11DeviceContext> ctx; dev->GetImmediateContext(&ctx);
    auto* tex = reinterpret_cast<ID3D11Texture2D*>(wf.texture);
    int w = 0, h = 0, stride = 0;
    if (!ReadbackTextureBGRA(dev, ctx.Get(), tex, bgra, w, h, stride)) return false;
    auto profile = settings_.ToProfile();
    BgraToNv12(bgra.data(), w, h, stride, nv12, profile.width, profile.height);
    VideoFrameData vfd{};
    vfd.data = nv12.data();
    vfd.width = profile.width; vfd.height = profile.height; vfd.stride = profile.width;
    vfd.timestamp_100ns = now_100ns;
    return writer_.WriteVideoFrame(vfd);
}

void RecordingSession::WriteAudioTick(int64_t now_100ns, std::vector<uint8_t>& buf) {
    mixer_.ReadMixed(buf.data(), buf.size());
    AudioSamples as{};
    as.data = buf.data();
    as.size_bytes = (uint32_t)buf.size();
    as.timestamp_100ns = now_100ns;
    writer_.WriteAudio(as);
}

#else
bool RecordingSession::Start(const RecordingSettings&, void*, unsigned long, std::string& error) { error = "Gravação suportada apenas no Windows"; return false; }
bool RecordingSession::Stop(std::string&) { return true; }
void RecordingSession::RunVideoLoop() {}
void RecordingSession::RunAudioLoop() {}
bool RecordingSession::WriteVideoTick(int64_t, std::vector<uint8_t>&, std::vector<uint8_t>&) { return false; }
void RecordingSession::WriteAudioTick(int64_t, std::vector<uint8_t>&) {}
#endif

} // namespace tft
