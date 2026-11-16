#pragma once

#include <string>
#include <vector>

#include "app/CommandProtocol.h"
#include "output/RecordingSettings.h"

namespace tft {

enum class RecorderState { Idle, Recording, Stopping, Error };

class RecorderController {
public:
    std::vector<std::string> HandleLine(const std::string& line);

    RecorderState GetState() const { return state_; }
    bool ShouldExit() const { return should_exit_; }
    const std::string& LastOutputPath() const { return last_output_path_; }

    // Para testes/integração: simula resultado da captura real.
    void SetStartHookResult(bool ok, std::string message, std::string path = "");

private:
    std::vector<std::string> HandleCommand(const Command& cmd);

    RecorderState state_ = RecorderState::Idle;
    bool should_exit_ = false;
    std::string last_error_;
    std::string last_output_path_;

    bool hook_set_ = false;
    bool hook_ok_ = true;
    std::string hook_message_;
    std::string hook_path_;
};

} // namespace tft
