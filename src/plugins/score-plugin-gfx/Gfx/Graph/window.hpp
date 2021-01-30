#pragma once

#include "renderstate.hpp"
#include "scene.hpp"

#include <QPlatformSurfaceEvent>
#include <QWindow>
#include <QTimer>
#include <QtGui/private/qrhigles2_p.h>

class Window : public QWindow
{

public:
  Window(GraphicsApi graphicsApi)
  {
    // Tell the platform plugin what we want.
    switch (graphicsApi)
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
  std::function<void()> onUpdate;
  std::function<void(QRhiCommandBuffer&)> onRender;
  std::function<void()> onResize;
  void init()
  {
    onWindowReady();
  }

  void resizeSwapChain()
  {
    if(swapChain)
    {
      m_hasSwapChain = swapChain->buildOrResize();
      state.size = swapChain->currentPixelSize();
      if (onResize)
        onResize();
    }
  }

  void releaseSwapChain()
  {
    if (swapChain && m_hasSwapChain)
    {
      m_hasSwapChain = false;
      swapChain->release();
    }
  }

  void render()
  {
    if (onUpdate)
      onUpdate();

    if (!swapChain)
      return;

    if (!m_hasSwapChain || m_notExposed)
    {
      requestUpdate();
      return;
    }

    if (swapChain->currentPixelSize() != swapChain->surfacePixelSize()
        || m_newlyExposed)
    {
      resizeSwapChain();
      if (!m_hasSwapChain)
        return;
      m_newlyExposed = false;
    }

    if (m_canRender)
    {
      QRhi::FrameOpResult r = state.rhi->beginFrame(swapChain, {});
      if (r == QRhi::FrameOpSwapChainOutOfDate)
      {
        resizeSwapChain();
        if (!m_hasSwapChain)
        {
          requestUpdate();
          return;
        }
        r = state.rhi->beginFrame(swapChain);
      }
      if (r != QRhi::FrameOpSuccess)
      {
        requestUpdate();
        return;
      }

      const auto commands = swapChain->currentFrameCommandBuffer();
      onRender(*commands);

      state.rhi->endFrame(swapChain, {});
    }
    else
    {
      QRhi::FrameOpResult r = state.rhi->beginFrame(swapChain, {});
      if (r == QRhi::FrameOpSwapChainOutOfDate)
      {
        resizeSwapChain();
        if (!m_hasSwapChain)
        {
          requestUpdate();
          return;
        }
        r = state.rhi->beginFrame(swapChain);
      }
      if (r != QRhi::FrameOpSuccess)
      {
        requestUpdate();
        return;
      }

      auto buf = swapChain->currentFrameCommandBuffer();
      auto batch = state.rhi->nextResourceUpdateBatch();
      buf->beginPass(swapChain->currentFrameRenderTarget(), Qt::black, {1.0f, 0}, batch);
      buf->endPass();

      state.rhi->endFrame(swapChain, {});
    }
    requestUpdate();
  }

  void exposeEvent(QExposeEvent*) override
  {
    if(!onWindowReady)
    {
      return;
    }
    if (isExposed() && !m_running)
    {
      m_running = true;
      init();
      resizeSwapChain();
    }

    const QSize surfaceSize = m_hasSwapChain ? swapChain->surfacePixelSize() : QSize();

    if ((!isExposed() || (m_hasSwapChain && surfaceSize.isEmpty())) && m_running)
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
  QRhiSwapChain* swapChain{};
  bool m_canRender{};

private:
  bool m_running = false;
  bool m_notExposed = false;
  bool m_newlyExposed = false;

  bool m_hasSwapChain{};
};
