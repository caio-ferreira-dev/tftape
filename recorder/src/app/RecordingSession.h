#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "audio/AudioMixer.h"
#include "capture/WgcCaptureSource.h"
#include "encode/IMp4Writer.h"
#include "output/RecordingSidecar.h"
#include "output/RecordingSettings.h"
#include "util/Clock.h"

namespace tft {

class IRoundDetector;

class RecordingSession {
public:
    // Injeção opcional das dependências (útil para testes/integração).
    // Quando nenhum detector é injetado, a sessão carrega tft_round_detector.json
    // (ou usa a configuração padrão) no início de cada gravação.
    explicit RecordingSession(std::shared_ptr<IMp4Writer> writer = nullptr,
                              std::shared_ptr<ISteadyClock> steady_clock = nullptr,
                              std::shared_ptr<IClock> system_clock = nullptr,
                              std::shared_ptr<IRoundDetector> round_detector = nullptr);
    ~RecordingSession();

    /// `warnings` recebe avisos de fontes opcionais indisponíveis (a gravação segue).
    bool Start(const RecordingSettings& settings, void* window_handle, unsigned long process_id,
               std::string& error, std::vector<std::string>* warnings = nullptr);
    bool Stop(std::string& error);

    bool IsRunning() const { return running_.load(); }
    const std::string& OutputPath() const { return output_path_; }
    /// Preenchido após Stop() com os dados que o controlador usa para gravar o sidecar.
    const RecordingSidecarData& Summary() const { return summary_; }

private:
    void RunVideoLoop();
    void RunAudioLoop();
    bool WriteVideoTick(int64_t now_100ns, std::vector<uint8_t>& bgra, std::vector<uint8_t>& nv12);
    void WriteAudioTick(int64_t now_100ns, std::vector<uint8_t>& buf);

    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::thread video_thread_;
    std::thread audio_thread_;
    RecordingSettings settings_{};
    std::string output_path_;
    std::shared_ptr<ISteadyClock> steady_clock_;
    std::shared_ptr<IClock> system_clock_;
    SteadyTimePoint start_steady_{};
    ClockTimePoint start_wall_{};
    RecordingSidecarData summary_;

    WgcCaptureSource capture_;
    std::shared_ptr<IMp4Writer> writer_;
    std::shared_ptr<IRoundDetector> round_detector_;
    AudioMixer mixer_;
    std::shared_ptr<class IAudioSource> game_src_;
    std::shared_ptr<class IAudioSource> sys_src_;
    std::shared_ptr<class IAudioSource> mic_src_;

    int64_t last_video_100ns_ = 0;
    std::string last_error_;
};

} // namespace tft
