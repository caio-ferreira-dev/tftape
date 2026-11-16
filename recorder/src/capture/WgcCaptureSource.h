#pragma once
#include "capture/ICaptureSource.h"
#include <cstdint>

namespace tft {

struct WgcFrame {
    void* texture = nullptr; // ID3D11Texture2D* (Windows)
    int width = 0;
    int height = 0;
    int64_t timestamp_100ns = 0;
};

// Captura de janela via Windows Graphics Capture (WGC). Implementação real em Windows.
class WgcCaptureSource : public ICaptureSource {
public:
    WgcCaptureSource();
    ~WgcCaptureSource() override;

    bool Start(void* window_handle) override;
    bool AcquireFrame(Frame& out) override;
    void Stop() override;

    // API estendida: obtém textura D3D11 do frame mais recente (quando disponível).
    bool AcquireTexture(WgcFrame& out);

    void* GetD3DDevice() const; // ID3D11Device*

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace tft
