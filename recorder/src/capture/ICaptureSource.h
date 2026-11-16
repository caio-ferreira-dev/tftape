#pragma once
#include <cstdint>

namespace tft {

struct Frame {
    int width = 0;
    int height = 0;
    int64_t timestamp_100ns = 0;
    // Dados de pixel/handle de textura dependem da implementação (WGC/D3D11).
};

class ICaptureSource {
public:
    virtual ~ICaptureSource() = default;
    virtual bool Start(void* window_handle) = 0;
    virtual bool AcquireFrame(Frame& out) = 0;
    virtual void Stop() = 0;
};

} // namespace tft
