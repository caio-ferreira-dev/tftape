#pragma once

#include <string>
#include <string_view>

namespace tft {

enum class Resolution { P720, P1080 };
enum class Framerate { F30, F60 };

struct AudioOptions {
    bool game_audio = true;      // padrão: som do jogo (TFT)
    bool system_audio = false;   // opcional: som de outras janelas/sistema
    bool microphone = false;     // opcional: microfone
};

struct VideoProfile {
    int width = 1280;
    int height = 720;
    int fps = 30;
    int video_bitrate = 4'000'000; // bits/s (padrão razoável)
    int audio_bitrate = 160'000;
};

struct RecordingSettings {
    Resolution resolution = Resolution::P720;
    Framerate framerate = Framerate::F30;
    AudioOptions audio{};
    std::string output_dir;

    bool IsValid(std::string* error = nullptr) const;
    VideoProfile ToProfile() const;
};

bool ParseResolution(std::string_view s, Resolution& out);
bool ParseFramerate(std::string_view s, Framerate& out);
std::string ToString(Resolution r);
std::string ToString(Framerate f);

} // namespace tft
