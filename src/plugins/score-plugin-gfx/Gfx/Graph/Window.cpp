#include <Gfx/Graph/Window.hpp>
#include <Gfx/Graph/Utils.hpp>
#include <Gfx/Settings/Model.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/gfx/Vulkan.hpp>

#include <core/application/ApplicationInterface.hpp>

#include <QGuiApplication>
#include <QPointer>
#include <QStringList>

#include <algorithm>

#if defined(__EMSCRIPTEN__)
#include <emscripten/em_asm.h>
#include <emscripten/emscripten.h>
#include <emscripten/val.h>

#include <string>
#endif
#include <QPlatformSurfaceEvent>
#include <QTimer>
#include <QtGui/private/qrhigles2_p.h>
#if QT_HAS_VULKAN
#if __has_include(<QtGui/private/qrhivulkan_p.h>)
#include <QtGui/private/qrhivulkan_p.h>
#else
#undef QT_HAS_VULKAN
#endif
#endif
#include <wobjectimpl.h>

W_OBJECT_IMPL(score::gfx::Window)
namespace score::gfx
{
namespace
{
std::vector<Window*> g_windows;

QString frameOpName(int r)
{
  switch(r)
  {
    case QRhi::FrameOpSuccess:
      return QStringLiteral("Success");
    case QRhi::FrameOpError:
      return QStringLiteral("Error");
    case QRhi::FrameOpSwapChainOutOfDate:
      return QStringLiteral("SwapChainOutOfDate");
    case QRhi::FrameOpDeviceLost:
      return QStringLiteral("DeviceLost");
    default:
      return QStringLiteral("<never called>");
  }
}

QString sizeStr(QSize s)
{
  return QStringLiteral("%1x%2").arg(s.width()).arg(s.height());
}

}

void installGfxDiagnostics()
{
#if defined(__EMSCRIPTEN__)
  static bool installed = false;
  if(installed)
    return;
  installed = true;

  // clang-format off
  EM_ASM({
    globalThis.__scoreGlEvents = [];
    var seen = new WeakSet();
    var describe = function(e) {
      if (!e) return "<null>";
      var s = e.tagName ? e.tagName.toLowerCase() : "?";
      if (e.id) s += "#" + e.id;
      var c = (typeof e.className === "string") ? e.className.trim() : "";
      if (c) s += "." + c.split(" ").filter(function(x) { return x.length; }).join(".");
      return s;
    };
    var attach = function(c) {
      if (!c || seen.has(c)) return;
      seen.add(c);
      var rec = function(type) {
        return function() {
          globalThis.__scoreGlEvents.push(
            new Date().toISOString() + "  " + type + "  on " + describe(c));
        };
      };
      c.addEventListener("webglcontextlost", rec("webglcontextlost"));
      c.addEventListener("webglcontextrestored", rec("webglcontextrestored"));
      c.addEventListener("webglcontextcreationerror", rec("webglcontextcreationerror"));
    };
    var roots = function() {
      var host = document.querySelector("#qt-shadow-container");
      var out = [document];
      if (host && host.shadowRoot) out.push(host.shadowRoot);
      return out;
    };
    var scan = function() {
      roots().forEach(function(r) {
        r.querySelectorAll("canvas").forEach(attach);
      });
    };
    scan();
    // Canvases appear when a window is shown, and the shadow root itself may
    // not exist yet: keep looking rather than sampling once.
    var tries = 0;
    var poll = function() {
      scan();
      if (++tries < 2000) setTimeout(poll, 500);
    };
    setTimeout(poll, 300);

    globalThis.__scoreGlState = function() {
      var lines = [];
      try {
        if (typeof GL !== "undefined" && GL.contexts) {
          // GL.contexts is an object keyed by handle, not an array: indexing it
          // by 0..length is how the first version of this counter managed to
          // report 0 live contexts while a GL window was rendering.
          var keys = Object.keys(GL.contexts);
          lines.push("  gl.tableEntries      : " + keys.length);
          var live = 0;
          keys.forEach(function(k) {
            var c = GL.contexts[k];
            if (!c) return;
            live++;
            var ctx = c.GLctx;
            lines.push("    ctx[" + k + "] canvas=" + describe(ctx ? ctx.canvas : null)
                       + " isContextLost="
                       + (ctx && ctx.isContextLost ? ctx.isContextLost() : "?"));
          });
          lines.push("  gl.liveContexts      : " + live
                     + "   currentContext=" + (GL.currentContext ? "yes" : "no"));
        } else {
          lines.push("  gl.contexts          : <emscripten GL table unavailable>");
        }
      } catch (e) {
        lines.push("  gl.contexts          : <error " + e + ">");
      }
      roots().forEach(function(r, ri) {
        var cv = r.querySelectorAll("canvas");
        lines.push("  canvases in " + (ri === 0 ? "document" : "shadowRoot") + " : " + cv.length);
        for (var i = 0; i < cv.length; i++) {
          var e = cv[i];
          var box = e.getBoundingClientRect();
          lines.push("    " + describe(e) + " attr=" + e.width + "x" + e.height
                     + " css=" + Math.round(box.width) + "x" + Math.round(box.height)
                     + " watched=" + seen.has(e));
        }
      });
      lines.push("  gl.contextEvents     : " + globalThis.__scoreGlEvents.length);
      globalThis.__scoreGlEvents.forEach(function(e) { lines.push("    " + e); });
      return lines.join("\n");
    };

    globalThis.scoreGfxDump = function() {
      var f = (typeof _score_gfx_dump_text !== "undefined")
                ? _score_gfx_dump_text
                : Module["_score_gfx_dump_text"];
      var s = UTF8ToString(f());
      console.log(s);
      return s;
    };
  });
  // clang-format on
#endif
}

const std::vector<Window*>& Window::allWindows() noexcept
{
  return g_windows;
}

QString Window::diagnosticState() const
{
  QStringList out;
  out << QStringLiteral("    window=%1 title=\"%2\" visible=%3 exposed=%4 geom=%5,%6 %7")
             .arg(QString::number(reinterpret_cast<quintptr>(this), 16), title())
             .arg(isVisible() ? "y" : "n")
             .arg(isExposed() ? "y" : "n")
             .arg(x())
             .arg(y())
             .arg(sizeStr(size()));

  out << QStringLiteral("      flags: running=%1 closed=%2 notExposed=%3 newlyExposed=%4 "
                        "hasSwapChain=%5 deviceLost=%6 canRender=%7")
             .arg(m_running ? "y" : "n", m_closed ? "y" : "n", m_notExposed ? "y" : "n",
                  m_newlyExposed ? "y" : "n", m_hasSwapChain ? "y" : "n",
                  m_deviceLost ? "y" : "n", m_canRender ? "y" : "n");

  out << QStringLiteral("      lastBeginFrame: %1   fps=%2")
             .arg(frameOpName(m_lastFrameOp))
             .arg(m_fps, 0, 'f', 1);

  if(state)
  {
    out << QStringLiteral("      state: rhi=%1 rhiDeviceLost=%2 samples=%3 renderSize=%4 "
                          "outputSize=%5 renderPassDescriptor=%6 renderList=%7")
               .arg(state->rhi ? QStringLiteral("yes") : QStringLiteral("NULL"),
                    state->rhi ? (state->rhi->isDeviceLost() ? "YES" : "no") : "?")
               .arg(state->samples)
               .arg(sizeStr(state->renderSize), sizeStr(state->outputSize),
                    state->renderPassDescriptor ? "yes" : "NULL",
                    state->renderer.expired() ? "NONE" : "yes");
  }
  else
  {
    out << QStringLiteral("      state: NULL  <- createRenderState() never ran");
  }

  if(m_swapChain)
  {
    out << QStringLiteral("      swapchain: current=%1 surface=%2 rt=%3")
               .arg(
                   sizeStr(m_swapChain->currentPixelSize()),
                   sizeStr(m_swapChain->surfacePixelSize()),
                   m_swapChain->currentFrameRenderTarget()
                       ? QStringLiteral("yes")
                       : QStringLiteral("NULL"));
  }
  else
  {
    out << QStringLiteral("      swapchain: NULL  <- nothing will ever be presented");
  }

  return out.join('\n');
}


Window::Window(GraphicsApi graphicsApi)
    : m_api{graphicsApi}
{
  g_windows.push_back(this);
  installGfxDiagnostics();
  setCursor(Qt::BlankCursor);

#if defined(__EMSCRIPTEN__)
  // No OS window manager on wasm: without this the output window drops behind
  // the main window when it's activated and can't be brought back.
  setFlag(Qt::WindowStaysOnTopHint, true);

  // The output window is a display surface, not somewhere to type. QWasmWindow
  // calls window()->requestActivate() on every pointer press on a top level
  // unless this flag is set (qwasmwindow.cpp, EventType::PointerDown), which
  // makes the output the focus window: from then on QWasmInputContext points
  // the IME at *its* canvas' hidden <input> and every keystroke meant for an
  // editor in the main window is delivered there instead. Combined with
  // WindowStaysOnTopHint above, the output would otherwise sit on top and
  // active, which is exactly the reported "typing breaks when a window device
  // is open". The cost is that Window::key/keyRelease no longer fire on wasm.
  setFlag(Qt::WindowDoesNotAcceptFocus, true);
#endif

  QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();

  // Tell the platform plugin what we want.
  switch(m_api)
  {
    default:
    case OpenGL:
#if QT_CONFIG(opengl)
      setSurfaceType(OpenGLSurface);
#if QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
      fmt = QRhiGles2InitParams::adjustedFormat();
#endif
#endif
    break;

#if QT_HAS_VULKAN
    case Vulkan:
      setSurfaceType(VulkanSurface);
      setVulkanInstance(score::gfx::staticVulkanInstance());
      break;
#endif

#if defined(_WIN32)
    case D3D11:
    case D3D12:
      setSurfaceType(Direct3DSurface);
      break;
#endif

#if defined(__APPLE__)
    case Metal:
      setSurfaceType(MetalSurface);
      break;
#endif
  }

  const auto& settings = score::AppContext().settings<Gfx::Settings::Model>();
  fmt.setSwapInterval(settings.getVSync() ? 1 : 0);

  switch(settings.getBuffers())
  {
    default:
    case 1:
      fmt.setSwapBehavior(QSurfaceFormat::SwapBehavior::SingleBuffer);
      break;
    case 2:
      fmt.setSwapBehavior(QSurfaceFormat::SwapBehavior::DoubleBuffer);
      break;
    case 3:
      fmt.setSwapBehavior(QSurfaceFormat::SwapBehavior::TripleBuffer);
      break;
  }

  const int samples = settings.resolveSamples(m_api);
  fmt.setSamples(samples);

  setFormat(fmt);

  if(auto platform = qGuiApp->platformName();
     platform.contains("eglfs") || platform.contains("vkkhr"))
    m_embeddedFullscreen = true;
}

Window::~Window()
{
  m_closed = true;
  std::erase(g_windows, this);
}

void Window::init()
{
  onWindowReady();
}

void Window::resizeSwapChain()
{
  if(m_swapChain)
  {
    const QSize surface = m_swapChain->surfacePixelSize();

    // QGles2SwapChain::createOrResize() returns true unconditionally, even for
    // an empty surface: it is not a usable "the swapchain is ready" signal.
    // Building the render list against an empty swapchain gives every node a
    // 1x1 render target (QRhi clamps empty texture sizes up) and a 0x0
    // viewport in the final blit, i.e. a permanently black window, since
    // nothing rebuilds the render list unless the surface size changes again.
    if(surface.isEmpty())
    {
      if(outputLogEnabled())
        qDebug() << "[gfxout] resizeSwapChain: win=" << (void*)this << "refused, empty surface"
                 << "current=" << m_swapChain->currentPixelSize();
      m_hasSwapChain = false;
      m_newlyExposed = true;
      requestUpdate();
      return;
    }

    m_hasSwapChain = m_swapChain->createOrResize();
    if(state)
      state->outputSize = m_swapChain->currentPixelSize();

    if(outputLogEnabled())
      qDebug() << "[gfxout] resizeSwapChain: win=" << (void*)this << "ok=" << m_hasSwapChain
               << "surface=" << surface << "current=" << m_swapChain->currentPixelSize()
               << "outputSize=" << (state ? state->outputSize : QSize{})
               << "rt=" << (void*)m_swapChain->currentFrameRenderTarget();

    if(onResize)
      onResize();
  }
  else
  {
    m_hasSwapChain = false;
  }
}

void Window::releaseSwapChain()
{
  if(m_swapChain && m_hasSwapChain)
  {
    if(outputLogEnabled())
      qDebug() << "[gfxout] releaseSwapChain: win=" << (void*)this;
    m_hasSwapChain = false;
    m_swapChain->destroy();

    // The render list is built against this swapchain: force a full rebuild
    // when the window comes back rather than reusing it as-is.
    m_newlyExposed = true;
  }
}

void Window::handleDeviceLost()
{
  if(m_deviceLost)
    return;

  m_deviceLost = true;
  m_hasSwapChain = false;
  m_canRender = false;

  // "QRhiGles2: Context is lost." is also emitted, benignly, whenever a QRhi is
  // destroyed -- ScreenNode::destroyOutput() -> RenderState::destroy() ->
  // ~QRhi() -> QRhiGles2::destroy() -> ensureContext(). That path never reaches
  // here (destroyOutput() clears m_swapChain first, so render() returns before
  // beginFrame), and neither does application shutdown, but say nothing on the
  // way out regardless: only a context that dies under a *live* window is a
  // defect worth reporting.
  if(m_closed || QCoreApplication::closingDown())
    return;

  qCritical() << "score::gfx::Window: the graphics context was lost while the output "
                 "was live. This output has stopped rendering.";

  if(onDeviceLost)
  {
    // Deferred, and not through this window: the handler is expected to
    // destroy and rebuild the output, i.e. to delete this.
    QPointer<Window> self{this};
    QMetaObject::invokeMethod(
        qApp,
        [self, cb = onDeviceLost] {
      if(self)
        cb();
    },
        Qt::QueuedConnection);
  }
}

bool Window::checkDeviceLost(int frameOpResult)
{
  m_lastFrameOp = frameOpResult;
  if(frameOpResult != QRhi::FrameOpDeviceLost
     && !(state && state->rhi && state->rhi->isDeviceLost()))
    return false;

  handleDeviceLost();
  return true;
}

void Window::render()
{
  static constexpr double fps_smoothing = .8;
  if(m_closed)
    return;

  // A lost context never comes back on its own: without this the window would
  // call beginFrame() on a dead QRhi on every update request and log
  // "QRhiGles2: Context is lost." forever.
  if(m_deviceLost)
    return;

  if(onUpdate)
  {
    onUpdate();
  }

  if(!m_swapChain)
    return;

  if(!m_hasSwapChain || m_notExposed)
  {
    // wasm delivers a one-shot expose (QWasmWindow::setVisible), so if the
    // surface had no size when exposeEvent latched m_notExposed, nothing ever
    // clears it again and the window stays black. Recover once it has a size.
    if(isExposed() && m_swapChain && !m_swapChain->surfacePixelSize().isEmpty())
    {
      if(outputLogEnabled())
        qDebug() << "[gfxout] render: win=" << (void*)this << "recovering, hasSwapChain=" << m_hasSwapChain
                 << "notExposed=" << m_notExposed
                 << "surface=" << m_swapChain->surfacePixelSize();
      m_notExposed = false;
      m_newlyExposed = true;
      // fall through: the resize block below will (re)create the swapchain
    }
    else
    {
      requestUpdate();
      return;
    }
  }

  if(m_swapChain->currentPixelSize() != m_swapChain->surfacePixelSize()
     || m_newlyExposed)
  {
    resizeSwapChain();
    if(!m_hasSwapChain)
    {
      requestUpdate();
      return;
    }
    m_newlyExposed = false;
  }

  if(m_canRender && state)
  {
    QRhi::FrameOpResult r = state->rhi->beginFrame(m_swapChain, {});
    if(checkDeviceLost(r))
      return;
    if(r == QRhi::FrameOpSwapChainOutOfDate)
    {
      resizeSwapChain();
      if(!m_hasSwapChain)
      {
        requestUpdate();
        return;
      }
      r = state->rhi->beginFrame(m_swapChain);
      if(checkDeviceLost(r))
        return;
    }
    if(r != QRhi::FrameOpSuccess)
    {
      requestUpdate();
      return;
    }

    const auto commands = m_swapChain->currentFrameCommandBuffer();
    onRender(*commands);

    state->rhi->endFrame(m_swapChain, {});
    {
      // 1. Calculate the time elapsed since the last frame
      if(const auto frame_ns = m_timer.nsecsElapsed(); frame_ns > 0)
      {
        const double fps = 1e9 / frame_ns;

        // 2. Smooth things a bit
        if(m_fps == 0.0f)
          m_fps = fps;
        else
          m_fps = (fps * fps_smoothing) + (m_fps * (1.0f - fps_smoothing));
      }
      m_timer.restart();
    }
  }
  else
  {
    QRhi::FrameOpResult r = state->rhi->beginFrame(m_swapChain, {});
    if(checkDeviceLost(r))
      return;
    if(r == QRhi::FrameOpSwapChainOutOfDate)
    {
      resizeSwapChain();
      if(!m_hasSwapChain)
      {
        requestUpdate();
        return;
      }
      r = state->rhi->beginFrame(m_swapChain);
      if(checkDeviceLost(r))
        return;
    }
    if(r != QRhi::FrameOpSuccess)
    {
      requestUpdate();
      return;
    }

    auto buf = m_swapChain->currentFrameCommandBuffer();
    auto batch = state->rhi->nextResourceUpdateBatch();
    buf->beginPass(m_swapChain->currentFrameRenderTarget(), Qt::black, {1.0f, 0}, batch);
    buf->endPass();

    state->rhi->endFrame(m_swapChain, {});
    m_fps = 0.;
  }

  if(m_fpsPushTimer.elapsed() > 50)
  {
    fps(m_fps);
    m_fpsPushTimer.restart();
  }

  if(this->onUpdate) {
    // requestUpdate is only to be used in the vsync case
    requestUpdate();
  }
}

void Window::exposeEvent(QExposeEvent* ev)
{
  if(!onWindowReady)
  {
    return;
  }
  if(outputLogEnabled())
    qDebug() << "[gfxout] exposeEvent: win=" << (void*)this << " exposed=" << isExposed() << "running=" << m_running
             << "notExposed=" << m_notExposed << "hasSwapChain=" << m_hasSwapChain
             << "size=" << size()
             << "surface=" << (m_swapChain ? m_swapChain->surfacePixelSize() : QSize{});

  if(isExposed() && !m_running)
  {
    m_running = true;
    init();
    resizeSwapChain();
  }

  if(m_hasSwapChain && !m_swapChain)
  {
    qDebug("exposeEvent: m_hasSwapChain && !m_swapChain");
    m_hasSwapChain = false;
  }

  const QSize surfaceSize = m_hasSwapChain ? m_swapChain->surfacePixelSize() : QSize();

  if((!isExposed() || (m_hasSwapChain && surfaceSize.isEmpty())) && m_running)
    m_notExposed = true;

  if(isExposed() && m_running && m_notExposed && !surfaceSize.isEmpty())
  {
    m_notExposed = false;
    m_newlyExposed = true;
  }

  if(isExposed())
  {
    m_closed = false;
  }

  if(isExposed() && !surfaceSize.isEmpty())
  {
    m_timer.restart();
    m_fpsPushTimer.restart();
    render();
  }
}

void Window::mouseDoubleClickEvent(QMouseEvent* ev)
{
  setWindowStates(windowStates() ^ Qt::WindowFullScreen);
}

bool Window::event(QEvent* e)
{
  switch(e->type())
  {
    case QEvent::UpdateRequest:
      render();
      break;

    case QEvent::TabletMove: {
      auto ev = static_cast<QTabletEvent*>(e);
      this->tabletMove(ev);
      this->interactiveEvent(e);
      break;
    }
    case QEvent::TabletPress:
    case QEvent::TabletRelease:
      this->interactiveEvent(e);
      break;

    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
      this->interactiveEvent(e);
      break;

    case QEvent::MouseMove: {
      auto ev = static_cast<QMouseEvent*>(e);
      this->mouseMove(ev->globalPosition(), ev->scenePosition());
      this->interactiveEvent(e);
      break;
    }
    case QEvent::KeyPress: {
      auto ev = static_cast<QKeyEvent*>(e);
      if(!ev->isAutoRepeat())
      {
        this->key(ev->key(), ev->text());
        this->interactiveEvent(e);
        if(ev->key() == Qt::Key_Escape)
          if(m_embeddedFullscreen)
            QMetaObject::invokeMethod(
                qGuiApp, [] { score::GUIApplicationInterface::instance().forceExit(); });
      }

      break;
    }
    case QEvent::KeyRelease: {
      auto ev = static_cast<QKeyEvent*>(e);
      if(!ev->isAutoRepeat())
      {
        this->keyRelease(ev->key(), ev->text());
        this->interactiveEvent(e);
      }
      break;
    }
    case QEvent::PlatformSurface:
      if(static_cast<QPlatformSurfaceEvent*>(e)->surfaceEventType()
         == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) // fallthrough
      case QEvent::Close: {
        releaseSwapChain();
        m_running = false;
        m_hasSwapChain = false;
        m_notExposed = true;
        m_closed = true;
        if(onClose)
          onClose();
#if defined(__EMSCRIPTEN__)
        score::reclaimMainWindowFocus();
#endif
      }
      break;

#if defined(__EMSCRIPTEN__)
    case QEvent::Hide:
      score::reclaimMainWindowFocus();
      break;
#endif

      default:
        break;
  }

  return QWindow::event(e);
}

}

#if defined(__EMSCRIPTEN__)
extern "C" EMSCRIPTEN_KEEPALIVE const char* score_gfx_dump_text()
{
  static std::string buf;

  QStringList out;
  const auto& windows = score::gfx::Window::allWindows();
  out << QStringLiteral("=== score gfx output dump ===");
  out << QStringLiteral("  outputs: %1").arg(windows.size());
  for(auto* w : windows)
    out << w->diagnosticState();

  emscripten::val fun = emscripten::val::global("__scoreGlState");
  if(!fun.isNull() && !fun.isUndefined())
  {
    emscripten::val res = fun();
    if(res.isString())
      out << QString::fromStdString(res.as<std::string>());
  }

  const QString text = out.join('\n');
  buf = text.toStdString();
  return buf.c_str();
}
#endif
