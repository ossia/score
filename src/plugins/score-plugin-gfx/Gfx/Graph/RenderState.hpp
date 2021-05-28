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
  RenderList* renderer{};

  QOffscreenSurface* surface{};
  QSize size{};
};
}
