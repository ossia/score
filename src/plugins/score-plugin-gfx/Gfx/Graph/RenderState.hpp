#pragma once
#include <QtGui/private/qrhi_p.h>

class QOffscreenSurface;
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
  Metal
};

class Window;

/**
 * @brief Global state associated to a rendering context.
 */
struct RenderState
{
  QRhi* rhi{};
  QRhiRenderPassDescriptor* renderPassDescriptor{};
  std::weak_ptr<RenderList> renderer{};

  QOffscreenSurface* surface{};
  QSize renderSize{};
  QSize outputSize{};
  GraphicsApi api{};
  QShaderVersion version{};
};
}
