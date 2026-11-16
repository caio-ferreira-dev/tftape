#include "audio/WasapiSystemLoopbackSource.h"

#include "audio/WasapiAudioClient.h"

namespace tft {

WasapiSystemLoopbackSource::WasapiSystemLoopbackSource(unsigned long exclude_process_id)
    : exclude_pid_(exclude_process_id) {}

WasapiSystemLoopbackSource::~WasapiSystemLoopbackSource() { Stop(); }

bool WasapiSystemLoopbackSource::Start() {
    Stop();
    client_ = std::make_unique<WasapiAudioClient>();

    // Exclui a árvore do processo do jogo para não duplicar áudio quando ambos os toggles estão on.
    if (exclude_pid_ != 0 && client_->OpenProcessLoopback(exclude_pid_, /*include_process_tree=*/false)) {
        isolated_ = true;
    } else if (!client_->OpenDefaultRenderLoopback()) {
        client_.reset();
        active_ = false;
        isolated_ = false;
        return false;
    } else {
        isolated_ = false;
    }

    active_ = true;
    return true;
}

void WasapiSystemLoopbackSource::Stop() {
    if (client_) {
        client_->Close();
        client_.reset();
    }
    active_ = false;
    isolated_ = false;
}

size_t WasapiSystemLoopbackSource::Read(uint8_t* dst, size_t max_bytes) {
    return client_ ? client_->Read(dst, max_bytes) : 0;
}

} // namespace tft
