#include "app/RecorderController.h"

#include <filesystem>
#include <utility>

#include "app/RecordingSession.h"
#include "capture/TftWindowFinder.h"
#include "output/OutputPath.h"
#include "output/RecordingSidecar.h"
#include "util/Logger.h"

namespace tft {

RecorderController::RecorderController() = default;
RecorderController::~RecorderController() {
    std::string ignored;
    FinalizeActiveRecording(ignored);
}

bool RecorderController::WriteLastSidecar(std::int64_t durationMs,
                                          const std::vector<RoundMarker>& rounds,
                                          std::vector<std::string>* warnings) {
    if (!has_last_recording_ || last_output_path_.empty()) return true;

    RecordingSidecarData data;
    data.videoFile = std::filesystem::path(last_output_path_).filename().string();
    data.createdAtUtc = FormatUtcIso8601(last_started_at_);
    data.durationMs = durationMs;
    data.resolution = ToString(last_settings_.resolution);
    data.fps = last_settings_.ToProfile().fps;
    data.rounds = rounds;

    std::string sidecarError;
    if (!WriteRecordingSidecar(data, last_output_path_, sidecarError)) {
        const std::string warning =
            "Gravação salva, mas não foi possível salvar os metadados: " + sidecarError;
        log::Warning(warning);
        if (warnings) warnings->push_back(warning);
        return false;
    }

    last_sidecar_path_ = SidecarPathFor(last_output_path_).string();
    last_duration_ms_ = durationMs;
    return true;
}

bool RecorderController::FinalizeActiveRecording(std::string& error,
                                                 std::vector<std::string>* warnings) {
    if (!session_) {
        if (state_ == RecorderState::Recording || state_ == RecorderState::Stopping) {
            state_ = RecorderState::Idle;
            // Modo hook (testes/integração): não há sessão, mas o sidecar ainda é gerado.
            WriteLastSidecar(0, {}, warnings);
        }
        return true;
    }

    if (!session_->Stop(error)) {
        state_ = RecorderState::Error;
        last_error_ = error;
        session_.reset();
        log::Error("Falha ao finalizar gravação: " + error);
        return false;
    }

    const auto summary = session_->Summary();
    session_.reset();
    state_ = RecorderState::Idle;
    WriteLastSidecar(summary.durationMs, summary.rounds, warnings);
    log::Info("Gravação finalizada: " + last_output_path_ +
              (last_sidecar_path_.empty() ? "" : " (sidecar: " + last_sidecar_path_ + ")"));
    return true;
}

void RecorderController::SetStartHookResult(bool ok, std::string message, std::string path) {
    hook_set_ = true;
    hook_ok_ = ok;
    hook_message_ = std::move(message);
    hook_path_ = std::move(path);
}

std::vector<std::string> RecorderController::HandleLine(const std::string& line) {
    std::string error;
    auto cmd = CommandProtocol::ParseLine(line, &error);
    if (!cmd) {
        if (error.empty()) error = "Comando inválido";
        return {CommandProtocol::Error("error", error).ToJsonLine()};
    }
    return HandleCommand(*cmd);
}

std::vector<std::string> RecorderController::HandleCommand(const Command& cmd) {
    std::vector<std::string> out;
    switch (cmd.type) {
        case CommandType::Ping:
            out.push_back(CommandProtocol::Ok("pong", "alive").ToJsonLine());
            break;
        case CommandType::Shutdown: {
            // Finaliza o mux e o sidecar antes de sair, inclusive se a UI fechar gravando.
            std::string ferr;
            std::vector<std::string> finalizeWarnings;
            if (!FinalizeActiveRecording(ferr, &finalizeWarnings)) {
                out.push_back(CommandProtocol::Error("error", ferr).ToJsonLine());
            }
            for (const auto& warning : finalizeWarnings) {
                out.push_back(CommandProtocol::Ok("warning", warning).ToJsonLine());
            }
            should_exit_ = true;
            out.push_back(CommandProtocol::Ok("shutdown", "bye").ToJsonLine());
            break;
        }
        case CommandType::Start: {
            if (state_ == RecorderState::Recording || state_ == RecorderState::Stopping) {
                out.push_back(CommandProtocol::Error("error", "Gravação já em andamento").ToJsonLine());
                break;
            }
            std::string err;
            if (!cmd.settings.IsValid(&err)) {
                state_ = RecorderState::Error;
                last_error_ = err;
                out.push_back(CommandProtocol::Error("error", err).ToJsonLine());
                break;
            }
            if (!OutputPath::EnsureDirectory(cmd.settings.output_dir, &err)) {
                state_ = RecorderState::Error;
                last_error_ = err;
                out.push_back(CommandProtocol::Error("error", err).ToJsonLine());
                break;
            }

            // Base do sidecar: configuração e instante de início desta gravação.
            last_settings_ = cmd.settings;
            last_started_at_ = SystemClockInstance().Now();
            has_last_recording_ = true;
            last_sidecar_path_.clear();
            last_duration_ms_ = 0;

            // Simulação completa (testes/integração controlada).
            if (hook_set_) {
                if (!hook_ok_) {
                    state_ = RecorderState::Error;
                    last_error_ = hook_message_;
                    out.push_back(CommandProtocol::Error("error", hook_message_).ToJsonLine());
                    break;
                }
                last_output_path_ = hook_path_.empty()
                    ? OutputPath::MakeUniqueMp4Path(cmd.settings.output_dir).string()
                    : hook_path_;
                state_ = RecorderState::Recording;
                out.push_back(CommandProtocol::Ok("recording_started", "Gravação iniciada", last_output_path_).ToJsonLine());
                break;
            }

            // Detecção da janela do TFT (somente este alvo é permitido).
            {
                TftWindowFinder finder;
                namespace fs = std::filesystem;
                if (!rules_path_.empty()) {
                    std::string ferr;
                    if (finder.LoadRulesFromFile(rules_path_, &ferr)) {
                        log::Info("Regras de janela carregadas: " + rules_path_);
                    } else {
                        log::Warning("Falha ao carregar regras de janela '" + rules_path_ + "': " + ferr + "; usando padrão");
                    }
                } else {
                    const char* defaults[] = {"tft_window_rules.json", "config/tft_window_rules.json", "recorder/config/tft_window_rules.json"};
                    for (const char* p : defaults) {
                        if (fs::exists(p)) {
                            std::string ferr;
                            if (finder.LoadRulesFromFile(p, &ferr)) {
                                log::Info("Regras de janela carregadas: " + std::string(p));
                            } else {
                                log::Warning("Falha ao carregar regras '" + std::string(p) + "': " + ferr + "; usando padrão");
                            }
                            break;
                        }
                    }
                }
                auto match = finder.Find();
                if (match.status != MatchStatus::Matched || !match.window) {
                    state_ = RecorderState::Error;
                    last_error_ = match.reason;
                    out.push_back(CommandProtocol::Error("error", match.reason).ToJsonLine());
                    break;
                }
                session_ = std::make_unique<RecordingSession>();
                std::string serr;
                std::vector<std::string> warnings;
                void* hwnd = reinterpret_cast<void*>(static_cast<uintptr_t>(match.window->handle_id));
                if (!session_->Start(cmd.settings, hwnd, match.window->process_id, serr, &warnings)) {
                    log::Error("Falha ao iniciar sessão: " + serr);
                    state_ = RecorderState::Error;
                    last_error_ = serr;
                    session_.reset();
                    out.push_back(CommandProtocol::Error("error", serr).ToJsonLine());
                    break;
                }
                for (const auto& warning : warnings) {
                    out.push_back(CommandProtocol::Ok("warning", warning).ToJsonLine());
                }
                last_output_path_ = session_->OutputPath();
            }

            state_ = RecorderState::Recording;
            out.push_back(CommandProtocol::Ok("recording_started", "Gravação iniciada (janela do TFT detectada)", last_output_path_).ToJsonLine());
            break;
        }
        case CommandType::Stop: {
            if (state_ != RecorderState::Recording) {
                out.push_back(CommandProtocol::Error("error", "Nenhuma gravação em andamento").ToJsonLine());
                break;
            }
            state_ = RecorderState::Stopping;
            std::string serr;
            std::vector<std::string> finalizeWarnings;
            if (!FinalizeActiveRecording(serr, &finalizeWarnings)) {
                out.push_back(CommandProtocol::Error("error", serr).ToJsonLine());
                break;
            }
            auto stopped = CommandProtocol::Ok("recording_stopped", "Gravação finalizada",
                                               last_output_path_);
            stopped.sidecar_path = last_sidecar_path_;
            stopped.duration_ms = last_duration_ms_;
            out.push_back(stopped.ToJsonLine());
            for (const auto& warning : finalizeWarnings) {
                out.push_back(CommandProtocol::Ok("warning", warning).ToJsonLine());
            }
            break;
        }
        case CommandType::Unknown:
        default:
            out.push_back(CommandProtocol::Error("error", "Comando desconhecido").ToJsonLine());
            break;
    }
    return out;
}

} // namespace tft
