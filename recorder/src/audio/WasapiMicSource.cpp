#include "audio/WasapiMicSource.h"

#include "audio/WasapiAudioClient.h"

namespace tft {

// Definido aqui (e não inline no header) porque o unique_ptr<WasapiAudioClient>
// exige o tipo completo para instanciar o destrutor.
WasapiMicSource::WasapiMicSource() = default;

WasapiMicSource::~WasapiMicSource() { Stop(); }

bool WasapiMicSource::Start() {
    Stop();
    client_ = std::make_unique<WasapiAudioClient>();
    if (!client_->OpenCaptureEndpoint()) {
        client_.reset();
        active_ = false;
        return false;
    }
    active_ = true;
    return true;
}

void WasapiMicSource::Stop() {
    if (client_) {
        client_->Close();
        client_.reset();
    }
    active_ = false;
}

size_t WasapiMicSource::Read(uint8_t* dst, size_t max_bytes) {
    return client_ ? client_->Read(dst, max_bytes) : 0;
}

} // namespace tft
