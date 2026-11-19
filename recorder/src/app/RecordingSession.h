#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "audio/AudioMixer.h"
#include "capture/WgcCaptureSource.h"
#include "encode/IMp4Writer.h"
#include "output/RecordingSettings.h"

namespace tft {

class RecordingSession {
public:
    // Injeção opcional do writer (útil para testes/integração); por padrão usa MfMp4Writer.
    explicit RecordingSession(std::shared_ptr<IMp4Writer> writer = nullptr);
    ~RecordingSession();

    /// `warnings` recebe avisos de fontes opcionais indisponíveis (a gravação segue).
    bool Start(const RecordingSettings& settings, void* window_handle, unsigned long process_id,
               std::string& error, std::vector<std::string>* warnings = nullptr);
    bool Stop(std::string& error);

    bool IsRunning() const { return running_.load(); }
    const std::string& OutputPath() const { return output_path_; }

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

    WgcCaptureSource capture_;
    std::shared_ptr<IMp4Writer> writer_;
    AudioMixer mixer_;
    std::shared_ptr<class IAudioSource> game_src_;
    std::shared_ptr<class IAudioSource> sys_src_;
    std::shared_ptr<class IAudioSource> mic_src_;

    int64_t start_time_100ns_ = 0;
    int64_t last_video_100ns_ = 0;
    std::string last_error_;
};

} // namespace tft
