#include "audio/WasapiSystemLoopbackSource.h"

namespace tft {
#if defined(_WIN32)
// TODO: implementar loopback do dispositivo de saída padrão (IAudioClient eCapture/eLoopback) e resample para PCM 48k estéreo.
bool WasapiSystemLoopbackSource::Start() { active_ = false; return true; }
void WasapiSystemLoopbackSource::Stop() { active_ = false; }
size_t WasapiSystemLoopbackSource::Read(uint8_t*, size_t) { return 0; }
#else
bool WasapiSystemLoopbackSource::Start() { active_ = false; return false; }
void WasapiSystemLoopbackSource::Stop() { active_ = false; }
size_t WasapiSystemLoopbackSource::Read(uint8_t*, size_t) { return 0; }
#endif
} // namespace tft
