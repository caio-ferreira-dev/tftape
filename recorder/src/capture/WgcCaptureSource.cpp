#include "capture/WgcCaptureSource.h"

#if defined(_WIN32)
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <inspectable.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>
#include <mutex>
#include <atomic>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;
namespace WG = winrt::Windows::Graphics;
namespace WGC = winrt::Windows::Graphics::Capture;
namespace WGD = winrt::Windows::Graphics::DirectX;
namespace WGD3D = winrt::Windows::Graphics::DirectX::Direct3D11;

namespace tft {

namespace {
// IGraphicsCaptureSession3 (Windows 11 22H2+) expõe IsBorderRequired e permite remover a
// borda amarela de captura em apps Win32 não empacotados. Declaramos a interface via ABI
// para não depender da versão do Windows SDK instalada no build.
struct __declspec(uuid("f2cdd966-22ae-5ea1-9596-3a289344c3be")) IGraphicsCaptureSession3
    : ::IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_IsBorderRequired(boolean* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsBorderRequired(boolean value) = 0;
};

void TryDisableCaptureBorder(const WGC::GraphicsCaptureSession& session) {
    try {
        auto* unk = winrt::get_unknown(session);
        if (!unk) return;
        Microsoft::WRL::ComPtr<IGraphicsCaptureSession3> s3;
        // Em versões anteriores a Windows 11 22H2 o QueryInterface falha; borda permanece.
        if (SUCCEEDED(unk->QueryInterface(IID_PPV_ARGS(&s3))) && s3)
            s3->put_IsBorderRequired(false);
    } catch (...) {}
}
} // namespace

struct WgcCaptureSource::Impl {
    ComPtr<ID3D11Device> d3dDevice;
    ComPtr<ID3D11DeviceContext> d3dContext;
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice device{ nullptr };
    WGC::GraphicsCaptureItem item{ nullptr };
    WGC::Direct3D11CaptureFramePool framePool{ nullptr };
    WGC::GraphicsCaptureSession session{ nullptr };
    WGC::Direct3D11CaptureFrame latest{ nullptr };
    winrt::event_token frameArrivedToken;
    std::mutex mutex;
    std::atomic<bool> running{false};
    int width = 0;
    int height = 0;

    static ComPtr<ID3D11Texture2D> GetTextureFromFrame(const WGC::Direct3D11CaptureFrame& frame) {
        auto surface = frame.Surface();
        ComPtr<ID3D11Texture2D> tex;
        auto access = surface.as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
        if (access) access->GetInterface(IID_PPV_ARGS(&tex));
        return tex;
    }
};

WgcCaptureSource::WgcCaptureSource() : impl_(new Impl()) {}
WgcCaptureSource::~WgcCaptureSource() { Stop(); delete impl_; }

bool WgcCaptureSource::Start(void* window_handle) {
    if (!window_handle) return false;
    Stop();
    HWND hwnd = reinterpret_cast<HWND>(window_handle);

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL fl;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels, 2, D3D11_SDK_VERSION,
                                   &impl_->d3dDevice, &fl, &impl_->d3dContext);
    if (FAILED(hr)) return false;

    ComPtr<IDXGIDevice> dxgi;
    hr = impl_->d3dDevice.As(&dxgi);
    if (FAILED(hr)) return false;

    // Criar IDirect3DDevice WinRT a partir do DXGI device
    ComPtr<IInspectable> inspectable;
    hr = CreateDirect3D11DeviceFromDXGIDevice(dxgi.Get(), &inspectable);
    if (FAILED(hr)) return false;
    winrt::copy_from_abi(impl_->device, inspectable.Get());

    // GraphicsCaptureItem a partir do HWND
    auto interop = winrt::get_activation_factory<WGC::GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
    hr = interop->CreateForWindow(hwnd, winrt::guid_of<WGC::IGraphicsCaptureItem>(),
                                  winrt::put_abi(impl_->item));
    if (FAILED(hr)) return false;

    auto size = impl_->item.Size();
    impl_->width = size.Width;
    impl_->height = size.Height;
    if (impl_->width <= 0 || impl_->height <= 0) return false;

    impl_->framePool = WGC::Direct3D11CaptureFramePool::CreateFreeThreaded(
        impl_->device, WGD::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, size);

    impl_->frameArrivedToken = impl_->framePool.FrameArrived([this](auto& pool, auto&) {
        auto frame = pool.TryGetNextFrame();
        if (!frame) return;
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->latest = frame;
        auto s = frame.ContentSize();
        impl_->width = s.Width; impl_->height = s.Height;
    });

    impl_->session = impl_->framePool.CreateCaptureSession(impl_->item);
    // IsBorderRequired/IsCursorCapture podem ser ajustados (Win10/11 variações)
    try { impl_->session.IsCursorCaptureEnabled(false); } catch (...) {}
    // Remove a borda amarela/limite de captura ao redor da janela (Windows 11 22H2+).
    TryDisableCaptureBorder(impl_->session);
    impl_->session.StartCapture();
    impl_->running = true;
    return true;
}

bool WgcCaptureSource::AcquireFrame(Frame& out) {
    WgcFrame wf{};
    if (!AcquireTexture(wf)) return false;
    out.width = wf.width; out.height = wf.height; out.timestamp_100ns = wf.timestamp_100ns;
    return true;
}

bool WgcCaptureSource::AcquireTexture(WgcFrame& out) {
    if (!impl_ || !impl_->running) return false;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->latest) return false;
    auto tex = Impl::GetTextureFromFrame(impl_->latest);
    if (!tex) return false;
    // não incrementa ref para out; consumidor deve usar enquanto frame válido/copiar.
    out.texture = tex.Get();
    out.width = impl_->width;
    out.height = impl_->height;
    out.timestamp_100ns = impl_->latest.SystemRelativeTime().count(); // 100ns
    // mantém latest vivo; para cópia segura o consumidor deve copiar a textura.
    return true;
}

void* WgcCaptureSource::GetD3DDevice() const { return impl_ ? impl_->d3dDevice.Get() : nullptr; }

void WgcCaptureSource::Stop() {
    if (!impl_) return;
    impl_->running = false;
    try {
        if (impl_->framePool) impl_->framePool.FrameArrived(impl_->frameArrivedToken);
        if (impl_->session) impl_->session.Close();
        if (impl_->framePool) impl_->framePool.Close();
    } catch (...) {}
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->latest = nullptr;
    impl_->session = nullptr;
    impl_->framePool = nullptr;
    impl_->item = nullptr;
    impl_->device = nullptr;
    impl_->d3dContext.Reset();
    impl_->d3dDevice.Reset();
}

} // namespace tft
#else
namespace tft {
struct WgcCaptureSource::Impl {};
WgcCaptureSource::WgcCaptureSource() : impl_(nullptr) {}
WgcCaptureSource::~WgcCaptureSource() {}
bool WgcCaptureSource::Start(void*) { return false; }
bool WgcCaptureSource::AcquireFrame(Frame&) { return false; }
bool WgcCaptureSource::AcquireTexture(WgcFrame&) { return false; }
void* WgcCaptureSource::GetD3DDevice() const { return nullptr; }
void WgcCaptureSource::Stop() {}
} // namespace tft
#endif
