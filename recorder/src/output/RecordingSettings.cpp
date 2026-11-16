#include "output/RecordingSettings.h"

namespace tft {

bool ParseResolution(std::string_view s, Resolution& out) {
    if (s == "720p" || s == "720") { out = Resolution::P720; return true; }
    if (s == "1080p" || s == "1080") { out = Resolution::P1080; return true; }
    return false;
}

bool ParseFramerate(std::string_view s, Framerate& out) {
    if (s == "30" || s == "30fps") { out = Framerate::F30; return true; }
    if (s == "60" || s == "60fps") { out = Framerate::F60; return true; }
    return false;
}

std::string ToString(Resolution r) {
    return r == Resolution::P1080 ? "1080p" : "720p";
}

std::string ToString(Framerate f) {
    return f == Framerate::F60 ? "60" : "30";
}

bool RecordingSettings::IsValid(std::string* error) const {
    if (output_dir.empty()) {
        if (error) *error = "output_dir vazio";
        return false;
    }
    // game_audio é o padrão; permitido estar true/false conforme origem, mas perfil deve ser consistente.
    return true;
}

VideoProfile RecordingSettings::ToProfile() const {
    VideoProfile p;
    if (resolution == Resolution::P1080) {
        p.width = 1920;
        p.height = 1080;
        p.video_bitrate = (framerate == Framerate::F60) ? 12'000'000 : 8'000'000;
    } else {
        p.width = 1280;
        p.height = 720;
        p.video_bitrate = (framerate == Framerate::F60) ? 6'000'000 : 4'000'000;
    }
    p.fps = (framerate == Framerate::F60) ? 60 : 30;
    p.audio_bitrate = 160'000;
    return p;
}

} // namespace tft
