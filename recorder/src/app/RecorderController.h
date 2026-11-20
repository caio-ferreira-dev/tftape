#pragma once

#include <memory>
#include <string>
#include <vector>

#include "app/CommandProtocol.h"
#include "output/RecordingSidecar.h"
#include "output/RecordingSettings.h"
#include "util/Clock.h"

namespace tft {

class RecordingSession;
enum class RecorderState { Idle, Recording, Stopping, Error };

class RecorderController {
public:
    RecorderController();
    ~RecorderController();

    std::vector<std::string> HandleLine(const std::string& line);

    /// Encerra a sessão ativa de forma explícita (finaliza o MP4 e grava o sidecar)
    /// em stop/shutdown/EOF. Chamado também no destrutor como melhor esforço.
    bool FinalizeActiveRecording(std::string& error,
                                 std::vector<std::string>* warnings = nullptr);

    RecorderState GetState() const { return state_; }
    bool ShouldExit() const { return should_exit_; }
    const std::string& LastOutputPath() const { return last_output_path_; }
    const std::string& LastSidecarPath() const { return last_sidecar_path_; }
    std::int64_t LastDurationMs() const { return last_duration_ms_; }

    // Caminho opcional para arquivo de regras de detecção da janela do TFT (JSON).
    void SetRulesPath(std::string path) { rules_path_ = std::move(path); }

    // Para testes/integração: simula resultado completo do início da gravação.
    void SetStartHookResult(bool ok, std::string message, std::string path = "");

private:
    std::vector<std::string> HandleCommand(const Command& cmd);
    bool WriteLastSidecar(std::int64_t durationMs,
                          const std::vector<RoundMarker>& rounds,
                          std::vector<std::string>* warnings);

    RecorderState state_ = RecorderState::Idle;
    bool should_exit_ = false;
    std::string last_error_;
    std::string last_output_path_;
    std::string last_sidecar_path_;
    std::int64_t last_duration_ms_ = 0;
    std::string rules_path_;
    RecordingSettings last_settings_{};
    ClockTimePoint last_started_at_{};
    bool has_last_recording_ = false;
    std::unique_ptr<RecordingSession> session_;

    bool hook_set_ = false;
    bool hook_ok_ = true;
    std::string hook_message_;
    std::string hook_path_;
};

} // namespace tft
