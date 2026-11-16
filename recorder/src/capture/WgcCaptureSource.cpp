#include "capture/WgcCaptureSource.h"

namespace tft {

#if defined(_WIN32)
// TODO: Implementar captura via Windows.Graphics.Capture + D3D11:
// - Criar GraphicsCaptureItem a partir do HWND do TFT
// - Criar Direct3D11CaptureFramePool e registrar callback de frame
// - Converter/copiar textura para o encoder (Media Foundation)
bool WgcCaptureSource::Start(void* /*window_handle*/) { return false; }
bool WgcCaptureSource::AcquireFrame(Frame& /*out*/) { return false; }
void WgcCaptureSource::Stop() {}
#else
bool WgcCaptureSource::Start(void* /*window_handle*/) { return false; }
bool WgcCaptureSource::AcquireFrame(Frame& /*out*/) { return false; }
void WgcCaptureSource::Stop() {}
#endif

} // namespace tft
