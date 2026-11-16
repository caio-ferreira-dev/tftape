#include "audio/WasapiAppLoopbackSource.h"

#if defined(_WIN32)
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <wrl/client.h>
#pragma comment(lib, "ole32.lib")
#endif

namespace tft {

WasapiAppLoopbackSource::WasapiAppLoopbackSource(unsigned long process_id) : pid_(process_id) {}

#if defined(_WIN32)
bool WasapiAppLoopbackSource::Start() {
    // Implementação base: loopback por processo (Windows 10 2004+ via ActivateAudioInterfaceAsync/
    // VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK). Aqui iniciamos modo ativo lógico; a captura real
    // requer inicialização COM/MMDevice/IAudioClient específica do PID e conversão de formato para PCM 48k estéreo.
    // TODO: implementar ativação por processo e leitura de pacotes no Read().
    active_ = false; // será true quando captura real estiver inicializada
    return true; // não falha a sessão; mixer usará silêncio se inativo
}
void WasapiAppLoopbackSource::Stop() { active_ = false; }
size_t WasapiAppLoopbackSource::Read(uint8_t*, size_t) { return 0; }
#else
bool WasapiAppLoopbackSource::Start() { active_ = false; return false; }
void WasapiAppLoopbackSource::Stop() { active_ = false; }
size_t WasapiAppLoopbackSource::Read(uint8_t*, size_t) { return 0; }
#endif

} // namespace tft
