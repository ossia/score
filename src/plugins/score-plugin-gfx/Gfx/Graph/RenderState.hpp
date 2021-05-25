#pragma once
#include <QtGui/private/qrhi_p.h>

class QOffscreenSurface;
namespace score::gfx
{
struct RenderList;
enum GraphicsApi
{
  Null,
  OpenGL,
  Vulkan,
  D3D11,
  Metal
};
class Window;
struct RenderState
{
  QRhi* rhi{};
  QRhiRenderPassDescriptor* renderPassDescriptor{};
  RenderList* renderer{};

  QOffscreenSurface* surface{};
  QSize size{};
};
}
