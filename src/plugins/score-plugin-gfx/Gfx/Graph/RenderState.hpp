#pragma once
#include <QOffscreenSurface>
#include <QtGui/private/qrhi_p.h>

#include <score_plugin_gfx_export.h>

#include <functional>

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
using QRhiBufferReadbackResult = QRhiReadbackResult;
#endif

class QOffscreenSurface;
class QWindow;
namespace score::gfx
{
class RenderList;
/**
 * @brief Available graphics APIs to use
 */
enum GraphicsApi
{
  Null,
  OpenGL,
  Vulkan,
  D3D11,
  Metal,
  D3D12
};

class Window;

/**
 * @brief Global state associated to a rendering context.
 */
struct RenderState
{
  RenderState() = default;
  RenderState(const RenderState&) = delete;
  RenderState(RenderState&&) = delete;
  RenderState& operator=(const RenderState&) = delete;
  RenderState& operator=(RenderState&&) = delete;

  QRhi* rhi{};
  QRhiRenderPassDescriptor* renderPassDescriptor{};
  std::weak_ptr<RenderList> renderer{};

  QOffscreenSurface* surface{};
  std::weak_ptr<score::gfx::Window>
      window{}; // Not always set, only used to get mouse events & such.
  QSize renderSize{};
  QSize outputSize{};
  int samples{1};
  QRhiTexture::Format renderFormat{QRhiTexture::RGBA8};
  GraphicsApi api{};
  QShaderVersion version{};

#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
  struct
  {
    bool drawIndirect{false};
    bool drawIndirectMulti{false};
  } caps;
#endif

  // Called after QRhi is destroyed to clean up an imported VkDevice
  std::function<void()> customDeviceCleanup;

  void destroy()
  {
    window.reset();

    delete rhi;
    rhi = nullptr;

    // Destroy imported VkDevice AFTER QRhi (which still references it during shutdown)
    if(customDeviceCleanup)
    {
      customDeviceCleanup();
      customDeviceCleanup = nullptr;
    }

    delete surface;
    surface = nullptr;
  }
};

SCORE_PLUGIN_GFX_EXPORT
std::shared_ptr<RenderState>
createRenderState(GraphicsApi graphicsApi, QSize sz, QWindow* window);

static const constexpr int32_t invalid_node_index = -1;
}
