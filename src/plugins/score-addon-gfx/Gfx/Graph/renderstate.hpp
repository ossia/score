#pragma once
#include <QtGui/private/qrhi_p.h>
#include <QtGui/private/qrhinull_p.h>

#ifndef QT_NO_OPENGL
#include <QOffscreenSurface>
#include <QtGui/private/qrhigles2_p.h>
#endif

#if QT_CONFIG(vulkan)
#include <QtGui/private/qrhivulkan_p.h>
#endif

#ifdef Q_OS_WIN
#include <QtGui/private/qrhid3d11_p.h>
#endif

#ifdef Q_OS_DARWIN
#include <QtGui/private/qrhimetal_p.h>
#endif

#include <QOffscreenSurface>
#include <QWindow>

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
  QRhiRenderBuffer* renderBuffer{};
  Renderer* renderer{};

  QOffscreenSurface* surface{};
  QSize size{};
  bool hasSwapChain = false;

/*
  static RenderState createOffscreen(GraphicsApi graphicsApi)
  {
    RenderState state;

    if (graphicsApi == Null)
    {
      QRhiNullInitParams params;
      state.rhi = QRhi::create(QRhi::Null, &params, {});
    }

#ifndef QT_NO_OPENGL
    if (graphicsApi == OpenGL)
    {
      state.surface = QRhiGles2InitParams::newFallbackSurface();
      QRhiGles2InitParams params;
      params.fallbackSurface = state.surface;
      state.rhi = QRhi::create(QRhi::OpenGLES2, &params, {});
    }
#endif

#if QT_CONFIG(vulkan)
    if (graphicsApi == Vulkan)
    {
      QRhiVulkanInitParams params;
      params.inst = staticVulkanInstance();
      state.rhi = QRhi::create(QRhi::Vulkan, &params, {});
    }
#endif

#ifdef Q_OS_WIN
    if (graphicsApi == D3D11)
    {
      QRhiD3D11InitParams params;
      params.enableDebugLayer = false;
      // if (framesUntilTdr > 0)
      // {
      //   params.framesUntilKillingDeviceViaTdr = framesUntilTdr;
      //   params.repeatDeviceKill = true;
      // }
      state.rhi = QRhi::create(QRhi::D3D11, &params, {});
    }
#endif

#ifdef Q_OS_DARWIN
    if (graphicsApi == Metal)
    {
      QRhiMetalInitParams params;
      state.rhi = QRhi::create(QRhi::Metal, &params, {});
      if (!state.rhi)
        qFatal("Failed to create METAL backend");
    }
#endif

    if (!state.rhi)
      qFatal("Failed to create RHI backend");

    return state;
  }*/
};
