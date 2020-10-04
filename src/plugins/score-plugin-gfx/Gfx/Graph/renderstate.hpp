#pragma once
#include <QtGui/private/qrhi_p.h>
struct Renderer;
enum GraphicsApi
{
  Null,
  OpenGL,
  Vulkan,
  D3D11,
  Metal
};
class Window;
class QOffscreenSurface;
struct RenderState
{
  QRhi* rhi{};
  QRhiRenderPassDescriptor* renderPassDescriptor{};
  Renderer* renderer{};

  QOffscreenSurface* surface{};
  QSize size{};
};
