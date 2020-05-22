#pragma once

#include "renderstate.hpp"
#include "scene.hpp"

#include <QPlatformSurfaceEvent>

class Window : public QWindow
{
  GraphicsApi m_graphicsApi{};

public:
  Window(GraphicsApi graphicsApi) : m_graphicsApi{graphicsApi}
  {
    // Tell the platform plugin what we want.
    switch (m_graphicsApi)
    {
      case OpenGL:
#if QT_CONFIG(opengl)
        setSurfaceType(OpenGLSurface);
        setFormat(QRhiGles2InitParams::adjustedFormat());
#endif
        break;
      case Vulkan:
        setSurfaceType(VulkanSurface);
        break;
      case D3D11:
        setSurfaceType(OpenGLSurface); // not a typo
        break;
      case Metal:
        setSurfaceType(MetalSurface);
        break;
      default:
        break;
    }
  }

  ~Window()
  {
  }

  std::function<void()> onWindowReady;
  std::function<void()> onRender;
  std::function<void()> onResize;
  bool canRender{};
  void init() { onWindowReady(); }

  void resizeSwapChain()
  {
    state.hasSwapChain = state.swapChain->buildOrResize(); // also handles m_ds
    state.size = state.swapChain->currentPixelSize();
    if (onResize)
      onResize();
  }

  void releaseSwapChain()
  {
    if (state.hasSwapChain)
    {
      state.hasSwapChain = false;
      state.swapChain->release();
    }
  }

  void render()
  {
    if (!state.hasSwapChain || m_notExposed)
    {
      requestUpdate();
      return;
    }

    if (state.swapChain->currentPixelSize() != state.swapChain->surfacePixelSize()
        || m_newlyExposed)
    {
      resizeSwapChain();
      if (!state.hasSwapChain)
        return;
      m_newlyExposed = false;
    }

    if (canRender)
    {
      QRhi::FrameOpResult r = state.rhi->beginFrame(state.swapChain, {});
      if (r == QRhi::FrameOpSwapChainOutOfDate)
      {
        resizeSwapChain();
        if (!state.hasSwapChain)
        {
          requestUpdate();
          return;
        }
        r = state.rhi->beginFrame(state.swapChain);
      }
      if (r != QRhi::FrameOpSuccess)
      {
        requestUpdate();
        return;
      }

      onRender();

      state.rhi->endFrame(state.swapChain, {});
    }
    else
    {
      QRhi::FrameOpResult r = state.rhi->beginFrame(state.swapChain, {});
      if (r == QRhi::FrameOpSwapChainOutOfDate)
      {
        resizeSwapChain();
        if (!state.hasSwapChain)
        {
          requestUpdate();
          return;
        }
        r = state.rhi->beginFrame(state.swapChain);
      }
      if (r != QRhi::FrameOpSuccess)
      {
        requestUpdate();
        return;
      }

      auto buf = state.swapChain->currentFrameCommandBuffer();
      auto batch = state.rhi->nextResourceUpdateBatch();
      buf->beginPass(state.swapChain->currentFrameRenderTarget(), Qt::black, {1.0f, 0}, batch);
      buf->endPass();

      state.rhi->endFrame(state.swapChain, {});
    }
    requestUpdate();
  }

  void exposeEvent(QExposeEvent*) override
  {
    if (isExposed() && !m_running)
    {
      m_running = true;
      init();
      resizeSwapChain();
    }

    const QSize surfaceSize = state.hasSwapChain ? state.swapChain->surfacePixelSize() : QSize();

    if ((!isExposed() || (state.hasSwapChain && surfaceSize.isEmpty())) && m_running)
      m_notExposed = true;

    if (isExposed() && m_running && m_notExposed && !surfaceSize.isEmpty())
    {
      m_notExposed = false;
      m_newlyExposed = true;
    }

    if (isExposed() && !surfaceSize.isEmpty())
      render();
  }

  void mouseDoubleClickEvent(QMouseEvent* ev) override
  {
    setWindowState(Qt::WindowState(windowState() ^ Qt::WindowFullScreen));
  }

  bool event(QEvent* e) override
  {
    switch (e->type())
    {
      case QEvent::UpdateRequest:
        render();
        break;

      case QEvent::PlatformSurface:
        if (static_cast<QPlatformSurfaceEvent*>(e)->surfaceEventType()
            == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
          releaseSwapChain();
        break;

      default:
        break;
    }

    return QWindow::event(e);
  }

  RenderState state;

private:
  bool m_running = false;
  bool m_notExposed = false;
  bool m_newlyExposed = false;
};
