#pragma once

#include <string>
#include <vector>

#include "output/RecordingSettings.h"

namespace tft {

/// Plano de fontes de áudio derivado das opções do usuário e do processo do TFT.
///
/// `game_isolation_pid`/`system_exclude_pid` valem 0 quando não há PID conhecido;
/// nesse caso as fontes usam loopback do dispositivo (fallback) e podem incluir
/// áudio de outras janelas.
struct AudioRoutingPlan {
    bool game = false;
    bool system = false;
    bool mic = false;
    unsigned long game_isolation_pid = 0;
    unsigned long system_exclude_pid = 0;
};

AudioRoutingPlan PlanAudioRouting(const AudioOptions& audio, unsigned long target_process_id);

/// Fontes que devem efetivamente permanecer no mix.
struct AudioSourceSelection {
    bool keep_game = false;
    bool keep_system = false;
};

/// Evita duplicar áudio quando o isolamento por processo não está disponível.
///
/// Sem isolamento, uma fonte "loopback do dispositivo" já contém todo o áudio do
/// sistema (incluindo o jogo). Manter a outra fonte duplicaria o sinal (distorção),
/// então apenas uma delas permanece no mix.
AudioSourceSelection DeduplicateAudioSources(const AudioRoutingPlan& plan,
                                             bool game_active, bool game_isolated,
                                             bool system_active, bool system_isolated);

/// Resultado da checagem pós-start das fontes de áudio.
struct AudioStartCheck {
    bool can_record = true;
    std::string error;               // preenchido apenas quando uma fonte obrigatória falha
    std::vector<std::string> warnings; // fontes opcionais indisponíveis: seguir sem elas
};

/// O som do jogo é obrigatório quando solicitado; sistema e microfone são opcionais.
AudioStartCheck EvaluateAudioStart(const AudioRoutingPlan& plan,
                                   bool game_active, bool system_active, bool mic_active);

} // namespace tft
