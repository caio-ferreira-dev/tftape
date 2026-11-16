#include "audio/AudioRouting.h"

namespace tft {

AudioRoutingPlan PlanAudioRouting(const AudioOptions& audio, unsigned long target_process_id) {
    AudioRoutingPlan plan;
    plan.game = audio.game_audio;
    plan.system = audio.system_audio;
    plan.mic = audio.microphone;

    if (target_process_id != 0) {
        // "Som do jogo": captura somente a árvore do processo do TFT.
        plan.game_isolation_pid = audio.game_audio ? target_process_id : 0;
        // "Outras janelas/sistema": tudo, exceto a árvore do TFT.
        plan.system_exclude_pid = audio.system_audio ? target_process_id : 0;
    }
    return plan;
}

AudioSourceSelection DeduplicateAudioSources(const AudioRoutingPlan& plan,
                                             bool game_active, bool game_isolated,
                                             bool system_active, bool system_isolated) {
    AudioSourceSelection selection;
    selection.keep_game = plan.game && game_active;
    selection.keep_system = plan.system && system_active;

    if (selection.keep_game && selection.keep_system) {
        if (!system_isolated) {
            // "Sistema" caiu para loopback do dispositivo: já inclui o jogo.
            selection.keep_game = false;
        } else if (!game_isolated) {
            // "Jogo" caiu para loopback do dispositivo: já inclui o sistema.
            selection.keep_system = false;
        }
    }
    return selection;
}

} // namespace tft
