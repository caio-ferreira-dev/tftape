#include "app/RecordingSession.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <memory>
#include <utility>
#include <vector>

#include "audio/AudioRouting.h"
#include "audio/WasapiAppLoopbackSource.h"
#include "audio/WasapiMicSource.h"
#include "audio/WasapiSystemLoopbackSource.h"
#include "encode/MfMp4Writer.h"
#include "output/OutputPath.h"
#include "rounds/IRoundDetector.h"
#include "rounds/RoundDetector.h"
#include "rounds/RoundDetectorConfigLoader.h"
#include "util/Logger.h"

#if defined(_WIN32)
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <mfapi.h>
using Microsoft::WRL::ComPtr;
#endif

namespace tft {

RecordingSession::RecordingSession(std::shared_ptr<IMp4Writer> writer,
                                   std::shared_ptr<ISteadyClock> steady_clock,
                                   std::shared_ptr<IClock> system_clock,
                                   std::shared_ptr<IRoundDetector> round_detector)
    : writer_(writer ? std::move(writer) : std::make_shared<MfMp4Writer>()),
      steady_clock_(steady_clock ? std::move(steady_clock)
                                 : std::make_shared<SteadyClock>()),
      system_clock_(system_clock ? std::move(system_clock)
                                 : std::make_shared<SystemClock>()),
      round_detector_(std::move(round_detector)) {}

RecordingSession::~RecordingSession() { std::string e; Stop(e); }

#if defined(_WIN32)
namespace {
int64_t ElapsedSinceStart(const ISteadyClock& clock, SteadyTimePoint start) {
    return Elapsed100ns(clock, start);
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

bool RecordingSession::Start(const RecordingSettings& settings, void* window_handle, unsigned long process_id,
                             std::string& error, std::vector<std::string>* warnings) {
    if (running_) { error = "Sessão já em execução"; return false; }
    settings_ = settings;
    stop_requested_ = false;
    std::string pathErr;
    if (!OutputPath::EnsureDirectory(settings_.output_dir, &pathErr)) { error = pathErr; return false; }
    output_path_ = OutputPath::MakeUniqueMp4Path(settings_.output_dir).string();

    if (!capture_.Start(window_handle)) { error = "Falha ao iniciar captura WGC da janela do TFT"; return false; }

    auto profile = settings_.ToProfile();
    if (!writer_->Open(output_path_, profile)) { error = writer_->LastError(); capture_.Stop(); return false; }

    // Áudio: som do jogo (padrão) + opções
    const AudioRoutingPlan audio_plan = PlanAudioRouting(settings_.audio, process_id);
    std::shared_ptr<WasapiAppLoopbackSource> game_source;
    std::shared_ptr<WasapiSystemLoopbackSource> system_source;
    std::shared_ptr<WasapiMicSource> mic_source;
    if (audio_plan.game) {
        game_source = std::make_shared<WasapiAppLoopbackSource>(audio_plan.game_isolation_pid);
        game_src_ = game_source;
        mixer_.SetGameSource(game_src_);
    }
    if (audio_plan.system) {
        system_source = std::make_shared<WasapiSystemLoopbackSource>(audio_plan.system_exclude_pid);
        sys_src_ = system_source;
        mixer_.SetSystemSource(sys_src_);
    }
    if (audio_plan.mic) {
        mic_source = std::make_shared<WasapiMicSource>();
        mic_src_ = mic_source;
        mixer_.SetMicSource(mic_src_);
    }
    mixer_.Start();

    // O áudio do jogo é obrigatório: nunca gravar silenciosamente sem ele.
    // Sistema e microfone são opcionais: falhas viram aviso para a UI.
    const AudioStartCheck audio_check = EvaluateAudioStart(
        audio_plan,
        game_source && game_source->IsActive(),
        system_source && system_source->IsActive(),
        mic_source && mic_source->IsActive());
    if (!audio_check.can_record) {
        error = audio_check.error;
        // Inclui o motivo específico da fonte do jogo antes de pará-la (Stop limpa o erro).
        if (game_source && !game_source->LastError().empty()) {
            error += " - " + game_source->LastError();
        }
        mixer_.Stop();
        mixer_.SetGameSource(nullptr);
        mixer_.SetSystemSource(nullptr);
        mixer_.SetMicSource(nullptr);
        game_src_.reset(); sys_src_.reset(); mic_src_.reset();
        capture_.Stop();
        writer_->Abort();
        return false;
    }
    if (warnings) *warnings = audio_check.warnings;

    // Rounds: configuração carregada uma vez por gravação. A ausência do arquivo
    // não é erro; uma configuração inválida gera aviso mas não impede a gravação.
    if (!round_detector_) {
        RoundDetectorConfig detector_config = DefaultRoundDetectorConfig();
        const std::string config_path = FindRoundDetectorConfigPath();
        std::string config_error;
        if (!config_path.empty()) {
            if (LoadRoundDetectorConfig(config_path, detector_config, &config_error)) {
                log::Debug("Detector de rounds usando " + config_path);
            } else {
                detector_config = DefaultRoundDetectorConfig();
                const std::string warning =
                    "Configuração de rounds inválida (" + config_error + "); usando padrão.";
                log::Warning(warning);
                if (warnings) warnings->push_back(warning);
            }
        } else {
            log::Debug("Configuração de rounds ausente; usando ROI padrão.");
        }
        round_detector_ = std::make_shared<RoundDetector>(std::move(detector_config));
    }
    round_detector_->Reset();

    // Sem isolamento por processo, o loopback do dispositivo já contém todo o áudio:
    // manter "jogo" e "sistema" juntos duplicaria o sinal (distorção).
    const AudioSourceSelection selection = DeduplicateAudioSources(
        audio_plan,
        game_source && game_source->IsActive(), game_source && game_source->IsProcessIsolated(),
        system_source && system_source->IsActive(), system_source && system_source->IsProcessIsolated());
    if (!selection.keep_game) {
        mixer_.SetGameSource(nullptr);
        game_src_.reset();
    }
    if (!selection.keep_system) {
        mixer_.SetSystemSource(nullptr);
        sys_src_.reset();
    }

    start_wall_ = system_clock_->Now();
    start_steady_ = steady_clock_->Now();
    last_video_100ns_ = 0;
    running_ = true;
    video_thread_ = std::thread([this]() { RunVideoLoop(); });
    audio_thread_ = std::thread([this]() { RunAudioLoop(); });
    return true;
}

bool RecordingSession::Stop(std::string& error) {
    if (!running_ && !video_thread_.joinable() && !audio_thread_.joinable()) return true;

    // Duração medida no mesmo relógio usado pelos timestamps de vídeo/áudio.
    const auto duration100ns = ElapsedSinceStart(*steady_clock_, start_steady_);

    stop_requested_ = true;
    running_ = false;
    if (audio_thread_.joinable()) audio_thread_.join();
    if (video_thread_.joinable()) video_thread_.join();
    mixer_.Stop();
    capture_.Stop();
    if (!writer_->Finalize()) { error = writer_->LastError(); writer_->Close(); return false; }
    writer_->Close();

    const auto profile = settings_.ToProfile();
    const std::int64_t durationMs = duration100ns / 10'000; // 100 ns -> ms

    summary_ = RecordingSidecarData{};
    summary_.videoFile = std::filesystem::path(output_path_).filename().string();
    summary_.createdAtUtc = FormatUtcIso8601(start_wall_);
    summary_.durationMs = durationMs;
    summary_.resolution = ToString(settings_.resolution);
    summary_.fps = profile.fps;

    // As threads já terminaram: todos os marcadores detectados estão disponíveis.
    if (round_detector_) {
        for (auto& detected : round_detector_->DrainDetections()) {
            RoundMarker marker;
            marker.label = std::move(detected.label);
            marker.startMs = detected.timestamp_100ns / 10'000;
            if (marker.startMs > durationMs) marker.startMs = durationMs;
            marker.source = std::move(detected.source);
            marker.confidence = detected.confidence;
            if (marker.label.empty()) continue;
            log::Info("Round detectado: " + marker.label + " em " +
                      std::to_string(marker.startMs) + " ms");
            summary_.rounds.push_back(std::move(marker));
        }
    }
    return true;
}

void RecordingSession::RunVideoLoop() {
    try {
        auto profile = settings_.ToProfile();
        const int64_t frameDur = 10'000'000LL / (profile.fps > 0 ? profile.fps : 30);
        std::vector<uint8_t> bgra, nv12;

        while (!stop_requested_) {
            int64_t now = ElapsedSinceStart(*steady_clock_, start_steady_);
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
            int64_t now = ElapsedSinceStart(*steady_clock_, start_steady_);
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
    if (round_detector_ && bgra.data() && w > 0 && h > 0 && stride > 0) {
        FrameView frame{};
        frame.bgra = bgra.data();
        frame.width = w;
        frame.height = h;
        frame.stride = stride;
        frame.timestamp_100ns = now_100ns;
        round_detector_->ProcessFrame(frame);
    }
    auto profile = settings_.ToProfile();
    BgraToNv12(bgra.data(), w, h, stride, nv12, profile.width, profile.height);
    VideoFrameData vfd{};
    vfd.data = nv12.data();
    vfd.width = profile.width; vfd.height = profile.height; vfd.stride = profile.width;
    vfd.timestamp_100ns = now_100ns;
    return writer_->WriteVideoFrame(vfd);
}

void RecordingSession::WriteAudioTick(int64_t now_100ns, std::vector<uint8_t>& buf) {
    mixer_.ReadMixed(buf.data(), buf.size());
    AudioSamples as{};
    as.data = buf.data();
    as.size_bytes = (uint32_t)buf.size();
    as.timestamp_100ns = now_100ns;
    writer_->WriteAudio(as);
}

#else
bool RecordingSession::Start(const RecordingSettings&, void*, unsigned long, std::string& error, std::vector<std::string>*) { error = "Gravação suportada apenas no Windows"; return false; }
bool RecordingSession::Stop(std::string&) { return true; }
void RecordingSession::RunVideoLoop() {}
void RecordingSession::RunAudioLoop() {}
bool RecordingSession::WriteVideoTick(int64_t, std::vector<uint8_t>&, std::vector<uint8_t>&) { return false; }
void RecordingSession::WriteAudioTick(int64_t, std::vector<uint8_t>&) {}
#endif

} // namespace tft
