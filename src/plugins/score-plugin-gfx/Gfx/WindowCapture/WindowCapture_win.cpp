#include <Gfx/WindowCapture/WindowCaptureBackend.hpp>

#if defined(_WIN32)

// Prevent IReference<boolean> / IReference<BYTE> redefinition in MinGW headers
#if defined(__MINGW32__)
#define ____FIReference_1_boolean_INTERFACE_DEFINED__ 1
#endif

#include <QDebug>

#include <dwmapi.h>
#include <windows.h>

#include <d3d11.h>
#include <dxgi.h>

// C++/WinRT headers for Windows.Graphics.Capture
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/Windows.Graphics.DirectX.h>

// Interop headers
#include <windows.graphics.capture.interop.h>
#if __has_include(<windows.graphics.directx.direct3d11.interop.h>)
#include <windows.graphics.directx.direct3d11.interop.h>
#else
// Manual declaration when the interop header is not available.
// CreateDirect3D11DeviceFromDXGIDevice wraps a DXGI device as a WinRT IDirect3DDevice.
extern "C" HRESULT __stdcall CreateDirect3D11DeviceFromDXGIDevice(
    IDXGIDevice* dxgiDevice, IInspectable** graphicsDevice);

// IDirect3DDxgiInterfaceAccess lets us extract native DXGI interfaces from WinRT surfaces.
// Use plain struct + __CRT_UUID_DECL instead of MIDL_INTERFACE so that
// MinGW's __uuidof() works in the constexpr context required by WinRT's guid_of<T>().
struct IDirect3DDxgiInterfaceAccess : public IUnknown
{
  virtual HRESULT STDMETHODCALLTYPE GetInterface(REFIID iid, void** p) = 0;
};
__CRT_UUID_DECL(IDirect3DDxgiInterfaceAccess, 0xA9B3D012, 0x3DF2, 0x4EE3, 0xB8, 0xD1, 0x86, 0x95, 0xF4, 0x57, 0xD3, 0xC1)

namespace Windows::Graphics::DirectX::Direct3D11
{
using IDirect3DDxgiInterfaceAccess = ::IDirect3DDxgiInterfaceAccess;
}
#endif

#include <inspectable.h>

#include <cstring>
#include <mutex>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "windowsapp.lib")

namespace Gfx::WindowCapture
{

// ── Window enumeration ────────────────────────────────────────────────

static std::vector<CapturableWindow>& enumerationResult()
{
  static thread_local std::vector<CapturableWindow> result;
  return result;
}

static BOOL CALLBACK enumWindowProc(HWND hwnd, LPARAM)
{
  if(!IsWindowVisible(hwnd))
    return TRUE;
  if(GetAncestor(hwnd, GA_ROOT) != hwnd)
    return TRUE;

  // Skip cloaked windows (UWP apps that are suspended, etc.)
  BOOL cloaked = FALSE;
  DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
  if(cloaked)
    return TRUE;

  wchar_t title[512]{};
  int len = GetWindowTextW(hwnd, title, 512);
  if(len <= 0)
    return TRUE;

  // Filter out tool windows
  LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
  if(exStyle & WS_EX_TOOLWINDOW)
    return TRUE;

  auto utf8 = QString::fromWCharArray(title, len).toStdString();
  enumerationResult().push_back({std::move(utf8), reinterpret_cast<uint64_t>(hwnd)});
  return TRUE;
}

// ── Monitor enumeration ───────────────────────────────────────────────

static std::vector<CapturableScreen>& monitorEnumerationResult()
{
  static thread_local std::vector<CapturableScreen> result;
  return result;
}

static BOOL CALLBACK enumMonitorProc(HMONITOR hMon, HDC, LPRECT lpRect, LPARAM)
{
  MONITORINFOEXW info{};
  info.cbSize = sizeof(info);
  if(!GetMonitorInfoW(hMon, &info))
    return TRUE;

  auto name = QString::fromWCharArray(info.szDevice).toStdString();

  CapturableScreen screen;
  screen.name = std::move(name);
  screen.id = reinterpret_cast<uint64_t>(hMon);
  screen.x = info.rcMonitor.left;
  screen.y = info.rcMonitor.top;
  screen.width = info.rcMonitor.right - info.rcMonitor.left;
  screen.height = info.rcMonitor.bottom - info.rcMonitor.top;
  monitorEnumerationResult().push_back(std::move(screen));
  return TRUE;
}

// ── D3D11 ↔ WinRT Direct3D interop helper ─────────────────────────

// Wraps an IDXGIDevice in a WinRT IDirect3DDevice using
// CreateDirect3D11DeviceFromDXGIDevice.
static winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice
createWinRTDevice(IDXGIDevice* dxgiDevice)
{
  winrt::com_ptr<::IInspectable> inspectable;
  winrt::check_hresult(
      CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice, inspectable.put()));
  return inspectable.as<
      winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
}

// Extracts ID3D11Texture2D from a WinRT IDirect3DSurface.
static winrt::com_ptr<ID3D11Texture2D> getSurfaceTexture(
    const winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface&
        surface)
{
  auto interop = surface.as<
      Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
  winrt::com_ptr<ID3D11Texture2D> tex;
  winrt::check_hresult(interop->GetInterface(IID_PPV_ARGS(tex.put())));
  return tex;
}

// ── Capture backend ────────────────────────────────────────────────

class WinGraphicsCaptureBackend final : public WindowCaptureBackend
{
public:
  WinGraphicsCaptureBackend() { winrt::init_apartment(winrt::apartment_type::multi_threaded); }

  ~WinGraphicsCaptureBackend() override { stop(); }

  bool available() const override
  {
    // Windows.Graphics.Capture requires Windows 10 version 1803+.
    // GraphicsCaptureSession::IsSupported() is the canonical check.
    try
    {
      return winrt::Windows::Graphics::Capture::GraphicsCaptureSession::
          IsSupported();
    }
    catch(...)
    {
      return false;
    }
  }

  bool supportsMode(CaptureMode mode) const override
  {
    switch(mode)
    {
      case CaptureMode::Window:
      case CaptureMode::SingleScreen:
        return true;
      case CaptureMode::AllScreens:
      case CaptureMode::Region:
        return false;
    }
    return false;
  }

  std::vector<CapturableWindow> enumerate() override
  {
    auto& result = enumerationResult();
    result.clear();
    EnumWindows(enumWindowProc, 0);
    return result;
  }

  std::vector<CapturableScreen> enumerateScreens() override
  {
    auto& result = monitorEnumerationResult();
    result.clear();
    EnumDisplayMonitors(nullptr, nullptr, enumMonitorProc, 0);
    return result;
  }

  bool start(const CaptureTarget& target) override
  {
    stop();

    try
    {
      // 1. Create D3D11 device
      if(!createD3D11Device())
        return false;

      // 2. Create capture item
      auto interopFactory
          = winrt::get_activation_factory<
                winrt::Windows::Graphics::Capture::GraphicsCaptureItem,
                IGraphicsCaptureItemInterop>();

      winrt::Windows::Graphics::Capture::GraphicsCaptureItem item{nullptr};

      switch(target.mode)
      {
        case CaptureMode::Window:
        {
          m_hwnd = reinterpret_cast<HWND>(target.windowId);
          if(!m_hwnd || !IsWindow(m_hwnd))
            return false;

          winrt::check_hresult(interopFactory->CreateForWindow(
              m_hwnd,
              winrt::guid_of<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>(),
              winrt::put_abi(item)));
          break;
        }

        case CaptureMode::SingleScreen:
        {
          HMONITOR hMonitor = reinterpret_cast<HMONITOR>(target.screenId);
          if(!hMonitor)
            return false;

          winrt::check_hresult(interopFactory->CreateForMonitor(
              hMonitor,
              winrt::guid_of<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>(),
              winrt::put_abi(item)));
          break;
        }

        default:
          // AllScreens and Region not supported
          return false;
      }

      m_item = item;

      // 3. Get initial size
      auto size = m_item.Size();
      m_width = size.Width;
      m_height = size.Height;

      if(m_width <= 0 || m_height <= 0)
      {
        qWarning() << "WindowCapture WGC: invalid capture size"
                    << m_width << "x" << m_height;
        return false;
      }

      // 4. Wrap DXGI device as WinRT IDirect3DDevice
      winrt::com_ptr<IDXGIDevice> dxgiDevice;
      winrt::check_hresult(m_d3dDevice->QueryInterface(
          IID_PPV_ARGS(dxgiDevice.put())));
      m_winrtDevice = createWinRTDevice(dxgiDevice.get());

      // 5. Create free-threaded frame pool (BGRA8, 2 buffers)
      m_framePool = winrt::Windows::Graphics::Capture::
          Direct3D11CaptureFramePool::CreateFreeThreaded(
              m_winrtDevice,
              winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
              2,
              {m_width, m_height});

      // 6. Create capture session
      m_session = m_framePool.CreateCaptureSession(m_item);

      // 7. Optionally disable yellow border (Win11 / 10 22H1+)
      configureCaptureSession();

      // 8. Create initial staging texture
      createStagingTexture(m_width, m_height);

      // 9. Start capture
      m_session.StartCapture();
      m_capturing = true;
      return true;
    }
    catch(winrt::hresult_error const& ex)
    {
      qWarning() << "WindowCapture WGC: start failed:"
                  << QString::fromWCharArray(ex.message().c_str());
      stop();
      return false;
    }
    catch(...)
    {
      qWarning() << "WindowCapture WGC: start failed (unknown exception)";
      stop();
      return false;
    }
  }

  void stop() override
  {
    m_capturing = false;

    try
    {
      if(m_session)
      {
        m_session.Close();
        m_session = nullptr;
      }
      if(m_framePool)
      {
        m_framePool.Close();
        m_framePool = nullptr;
      }
      if(m_winrtDevice)
      {
        m_winrtDevice.Close();
        m_winrtDevice = nullptr;
      }
    }
    catch(...)
    {
    }

    m_item = nullptr;
    m_stagingTexture = nullptr;
    m_d3dContext = nullptr;
    m_d3dDevice = nullptr;
    m_hwnd = nullptr;
    m_width = 0;
    m_height = 0;
    m_cpuBuffer.clear();
  }

  CapturedFrame grab() override
  {
    if(!m_capturing || !m_framePool)
      return {};

    try
    {
      auto frame = m_framePool.TryGetNextFrame();
      if(!frame)
        return {};

      // Check for resize
      auto contentSize = frame.ContentSize();
      int newW = contentSize.Width;
      int newH = contentSize.Height;

      if(newW <= 0 || newH <= 0)
      {
        frame.Close();
        return {};
      }

      if(newW != m_width || newH != m_height)
      {
        m_width = newW;
        m_height = newH;

        // Recreate frame pool with new size
        m_framePool.Recreate(
            m_winrtDevice,
            winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
            2,
            {m_width, m_height});

        // Recreate staging texture
        createStagingTexture(m_width, m_height);

        // This frame might be stale after Recreate; try to get a fresh one
        frame.Close();
        frame = m_framePool.TryGetNextFrame();
        if(!frame)
          return {};
      }

      // Get the D3D11 texture from the captured frame
      auto surface = frame.Surface();
      auto sourceTexture = getSurfaceTexture(surface);

      if(!sourceTexture || !m_stagingTexture)
      {
        frame.Close();
        return {};
      }

      // Copy the captured frame to our staging texture
      m_d3dContext->CopyResource(m_stagingTexture.get(), sourceTexture.get());

      // Map the staging texture for CPU read
      D3D11_MAPPED_SUBRESOURCE mapped{};
      HRESULT hr = m_d3dContext->Map(
          m_stagingTexture.get(), 0, D3D11_MAP_READ, 0, &mapped);
      if(FAILED(hr))
      {
        frame.Close();
        return {};
      }

      // Copy to our CPU buffer (the Map pointer is only valid until Unmap)
      const int rowBytes = m_width * 4;
      const size_t totalBytes = static_cast<size_t>(rowBytes) * m_height;
      if(m_cpuBuffer.size() != totalBytes)
        m_cpuBuffer.resize(totalBytes);

      const uint8_t* src = static_cast<const uint8_t*>(mapped.pData);
      uint8_t* dst = m_cpuBuffer.data();

      if(static_cast<int>(mapped.RowPitch) == rowBytes)
      {
        std::memcpy(dst, src, totalBytes);
      }
      else
      {
        // Row pitch may differ from width * 4 due to alignment
        for(int y = 0; y < m_height; ++y)
        {
          std::memcpy(dst + y * rowBytes, src + y * mapped.RowPitch, rowBytes);
        }
      }

      m_d3dContext->Unmap(m_stagingTexture.get(), 0);
      frame.Close();

      CapturedFrame result;
      result.type = CapturedFrame::CPU_BGRA;
      result.data = m_cpuBuffer.data();
      result.stride = rowBytes;
      result.width = m_width;
      result.height = m_height;
      return result;
    }
    catch(winrt::hresult_error const& ex)
    {
      qWarning() << "WindowCapture WGC: grab failed:"
                  << QString::fromWCharArray(ex.message().c_str());
      return {};
    }
    catch(...)
    {
      return {};
    }
  }

private:
  bool createD3D11Device()
  {
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };
    D3D_FEATURE_LEVEL featureLevel{};

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if !defined(NDEBUG)
    // Enable debug layer in debug builds if available
    // flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    ID3D11Device* rawDevice{};
    ID3D11DeviceContext* rawContext{};
    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &rawDevice,
        &featureLevel,
        &rawContext);

    if(FAILED(hr))
    {
      qWarning() << "WindowCapture WGC: D3D11CreateDevice failed, hr ="
                  << Qt::hex << hr;
      return false;
    }

    m_d3dDevice.attach(rawDevice);
    m_d3dContext.attach(rawContext);

    // Enable multithread protection since WGC frame pool is free-threaded
    winrt::com_ptr<ID3D10Multithread> multithread;
    m_d3dDevice.as(multithread);
    if(multithread)
      multithread->SetMultithreadProtected(TRUE);

    return true;
  }

  void configureCaptureSession()
  {
    // Try to enable cursor capture via IGraphicsCaptureSession2
    try
    {
      if(auto session2 = m_session.try_as<
             winrt::Windows::Graphics::Capture::IGraphicsCaptureSession2>())
      {
        session2.IsCursorCaptureEnabled(true);
      }
    }
    catch(...)
    {
    }

    // Try to disable the yellow capture border via IGraphicsCaptureSession3
    // Available on Windows 11 and Windows 10 22H1+
    try
    {
      if(auto session3 = m_session.try_as<
             winrt::Windows::Graphics::Capture::IGraphicsCaptureSession3>())
      {
        session3.IsBorderRequired(false);
      }
    }
    catch(...)
    {
    }
  }

  void createStagingTexture(int width, int height)
  {
    m_stagingTexture = nullptr;

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;

    ID3D11Texture2D* rawTex{};
    HRESULT hr = m_d3dDevice->CreateTexture2D(&desc, nullptr, &rawTex);
    if(SUCCEEDED(hr))
    {
      m_stagingTexture.attach(rawTex);
    }
    else
    {
      qWarning() << "WindowCapture WGC: CreateTexture2D (staging) failed, hr ="
                  << Qt::hex << hr;
    }
  }

  // WinRT capture objects
  winrt::Windows::Graphics::Capture::GraphicsCaptureItem m_item{nullptr};
  winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool{
      nullptr};
  winrt::Windows::Graphics::Capture::GraphicsCaptureSession m_session{nullptr};
  winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice m_winrtDevice{
      nullptr};

  // D3D11 device (owned by this backend)
  winrt::com_ptr<ID3D11Device> m_d3dDevice;
  winrt::com_ptr<ID3D11DeviceContext> m_d3dContext;

  // Staging texture for CPU readback (persisted between frames)
  winrt::com_ptr<ID3D11Texture2D> m_stagingTexture;

  // CPU buffer that outlives the Map/Unmap call
  std::vector<uint8_t> m_cpuBuffer;

  HWND m_hwnd{};
  int m_width{};
  int m_height{};
  bool m_capturing{};
};

std::unique_ptr<WindowCaptureBackend> createWindowCaptureBackend()
{
  return std::make_unique<WinGraphicsCaptureBackend>();
}

}

#endif
