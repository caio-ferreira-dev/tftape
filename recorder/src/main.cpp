#include <iostream>
#include <string>

#include "app/CommandProtocol.h"
#include "app/RecorderController.h"

// Host de comandos JSON-lines (stdin -> stdout).
// Em Windows, a implementação completa integra captura/áudio/encode; neste esqueleto
// o controlador gerencia estado e validação, emitindo eventos para a UI.
int main() {
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
    return 0;
}
