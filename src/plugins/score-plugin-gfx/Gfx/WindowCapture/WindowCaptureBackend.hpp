#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Gfx::WindowCapture
{

enum class CaptureMode : int
{
  Window = 0,
  AllScreens = 1,
  SingleScreen = 2,
  Region = 3
};

struct CapturedFrame
{
  enum Type
  {
    None,
    CPU_RGBA,
    CPU_BGRA,
    D3D11_Texture,
    IOSurface_Ref,
    DMA_BUF_FD
  };
  Type type{None};

  // CPU path
  const uint8_t* data{};
  int stride{};

  // GPU path: D3D11 ID3D11Texture2D*, or IOSurfaceRef
  void* nativeHandle{};

  // PipeWire DMA-BUF path
  int dmabufFd{-1};
  uint32_t drmFormat{};
  uint64_t drmModifier{};
  int dmabufStride{};
  int dmabufOffset{};

  int width{};
  int height{};
};

struct CapturableWindow
{
  std::string title;
  uint64_t id{};
};

struct CapturableScreen
{
  std::string name;
  uint64_t id{};
  int x{}, y{}, width{}, height{};
};

struct CaptureTarget
{
  CaptureMode mode{CaptureMode::Window};
  uint64_t windowId{};
  uint64_t screenId{};
  int regionX{}, regionY{}, regionW{}, regionH{};
};

struct WindowCaptureBackend
{
  virtual ~WindowCaptureBackend() = default;
  virtual bool available() const = 0;
  virtual bool supportsMode(CaptureMode mode) const = 0;
  virtual std::vector<CapturableWindow> enumerate() = 0;
  virtual std::vector<CapturableScreen> enumerateScreens() = 0;
  virtual bool start(const CaptureTarget& target) = 0;
  virtual void stop() = 0;
  virtual CapturedFrame grab() = 0;
};

// Returns the appropriate backend for the current platform and session type.
std::unique_ptr<WindowCaptureBackend> createWindowCaptureBackend();

}
