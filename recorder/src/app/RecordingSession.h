#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include "audio/AudioMixer.h"
#include "capture/WgcCaptureSource.h"
#include "encode/MfMp4Writer.h"
#include "output/RecordingSettings.h"

namespace tft {

class RecordingSession {
public:
    RecordingSession() = default;
    ~RecordingSession();

    bool Start(const RecordingSettings& settings, void* window_handle, unsigned long process_id, std::string& error);
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
    MfMp4Writer writer_;
    AudioMixer mixer_;
    std::shared_ptr<class IAudioSource> game_src_;
    std::shared_ptr<class IAudioSource> sys_src_;
    std::shared_ptr<class IAudioSource> mic_src_;

    int64_t start_time_100ns_ = 0;
    int64_t last_video_100ns_ = 0;
    std::string last_error_;
};

} // namespace tft
