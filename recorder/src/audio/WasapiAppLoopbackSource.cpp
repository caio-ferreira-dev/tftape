#include "audio/WasapiAppLoopbackSource.h"

#include "audio/WasapiAudioClient.h"

namespace tft {

WasapiAppLoopbackSource::WasapiAppLoopbackSource(unsigned long process_id) : pid_(process_id) {}

WasapiAppLoopbackSource::~WasapiAppLoopbackSource() { Stop(); }

bool WasapiAppLoopbackSource::Start() {
    Stop();

    // Sem endpoint de reprodução ativo, o loopback (por processo ou do dispositivo)
    // pode até ativar, mas não entrega áudio: recusamos gravar silenciosamente.
    if (!WasapiAudioClient::HasActiveRenderEndpoint()) {
        last_error_ = "Nenhum dispositivo de reprodução de áudio ativo";
        return false;
    }

    client_ = std::make_unique<WasapiAudioClient>();

    // Preferência: isolar apenas o processo do jogo e seus filhos.
    if (pid_ != 0 && client_->OpenProcessLoopback(pid_, /*include_process_tree=*/true)) {
        isolated_ = true;
    } else if (!client_->OpenDefaultRenderLoopback()) {
        // Sem isolamento e sem loopback do dispositivo: áudio do jogo indisponível.
        last_error_ = "Falha ao iniciar a captura de áudio do jogo";
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

void WasapiAppLoopbackSource::Stop() {
    if (client_) {
        client_->Close();
        client_.reset();
    }
    active_ = false;
    isolated_ = false;
    last_error_.clear();
}

size_t WasapiAppLoopbackSource::Read(uint8_t* dst, size_t max_bytes) {
    return client_ ? client_->Read(dst, max_bytes) : 0;
}

} // namespace tft
