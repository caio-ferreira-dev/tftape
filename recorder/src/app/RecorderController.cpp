#include "app/RecorderController.h"

#include "output/OutputPath.h"

namespace tft {

void RecorderController::SetStartHookResult(bool ok, std::string message, std::string path) {
    hook_set_ = true;
    hook_ok_ = ok;
    hook_message_ = std::move(message);
    hook_path_ = std::move(path);
}

std::vector<std::string> RecorderController::HandleLine(const std::string& line) {
    auto cmd = CommandProtocol::ParseLine(line);
    if (!cmd) {
        return {CommandProtocol::Error("error", "Comando inválido").ToJsonLine()};
    }
    return HandleCommand(*cmd);
}

std::vector<std::string> RecorderController::HandleCommand(const Command& cmd) {
    std::vector<std::string> out;
    switch (cmd.type) {
        case CommandType::Ping:
            out.push_back(CommandProtocol::Ok("pong", "alive").ToJsonLine());
            break;
        case CommandType::Shutdown:
            if (state_ == RecorderState::Recording) {
                state_ = RecorderState::Idle; // melhor esforço; implementação real finaliza mux
            }
            should_exit_ = true;
            out.push_back(CommandProtocol::Ok("shutdown", "bye").ToJsonLine());
            break;
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
            // Ponto de integração: aqui o core real localiza janela TFT e inicia pipeline.
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
            } else {
                last_output_path_ = OutputPath::MakeUniqueMp4Path(cmd.settings.output_dir).string();
            }
            state_ = RecorderState::Recording;
            out.push_back(CommandProtocol::Ok("recording_started", "Gravação iniciada", last_output_path_).ToJsonLine());
            break;
        }
        case CommandType::Stop:
            if (state_ != RecorderState::Recording) {
                out.push_back(CommandProtocol::Error("error", "Nenhuma gravação em andamento").ToJsonLine());
                break;
            }
            state_ = RecorderState::Stopping;
            // implementação real: parar captura, flush/mux finalize, fechar arquivo
            state_ = RecorderState::Idle;
            out.push_back(CommandProtocol::Ok("recording_stopped", "Gravação finalizada", last_output_path_).ToJsonLine());
            break;
        case CommandType::Unknown:
        default:
            out.push_back(CommandProtocol::Error("error", "Comando desconhecido").ToJsonLine());
            break;
    }
    return out;
}

} // namespace tft
