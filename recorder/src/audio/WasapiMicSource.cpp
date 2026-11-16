#include "audio/WasapiMicSource.h"

namespace tft {
#if defined(_WIN32)
// TODO: implementar captura do endpoint de entrada padrão (microfone) via WASAPI e conversão para PCM 48k estéreo.
bool WasapiMicSource::Start() { active_ = false; return true; }
void WasapiMicSource::Stop() { active_ = false; }
size_t WasapiMicSource::Read(uint8_t*, size_t) { return 0; }
#else
bool WasapiMicSource::Start() { active_ = false; return false; }
void WasapiMicSource::Stop() { active_ = false; }
size_t WasapiMicSource::Read(uint8_t*, size_t) { return 0; }
#endif
} // namespace tft
