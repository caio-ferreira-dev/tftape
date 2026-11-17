#include <exception>
#include <iostream>
#include <string>

#include "app/CommandProtocol.h"
#include "app/RecorderController.h"

// Host de comandos JSON-lines (stdin -> stdout).
// Em Windows, a implementação completa integra captura/áudio/encode; neste esqueleto
// o controlador gerencia estado e validação, emitindo eventos para a UI.
int main() {
    try {
        tft::RecorderController controller;
        std::string line;
        while (std::getline(std::cin, line)) {
            auto responses = controller.HandleLine(line);
            for (const auto& resp : responses) {
                std::cout << resp << std::endl;
            }
            if (controller.ShouldExit()) {
                break;
            }
        }

        // Fim do stdin (UI fechou sem enviar shutdown): finaliza o MP4 antes de sair.
        std::string error;
        if (!controller.FinalizeActiveRecording(error)) {
            std::cout << tft::CommandProtocol::Error("error", error).ToJsonLine() << std::endl;
        }
    } catch (const std::exception& ex) {
        std::cout << tft::CommandProtocol::Error("fatal", ex.what()).ToJsonLine() << std::endl;
        return 1;
    } catch (...) {
        std::cout << tft::CommandProtocol::Error("fatal", "Falha não identificada no gravador").ToJsonLine() << std::endl;
        return 1;
    }
    return 0;
}
