#pragma once
#include "capture/ICaptureSource.h"

namespace tft {

// Implementação Windows (WGC). Em plataformas não-Windows permanece como stub.
class WgcCaptureSource : public ICaptureSource {
public:
    bool Start(void* window_handle) override;
    bool AcquireFrame(Frame& out) override;
    void Stop() override;
};

} // namespace tft
