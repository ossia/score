#pragma once

// Shared rig for the *presented-swapchain* gfx tests.
//
// Everything else in tests/gfx/ renders into a BackgroundNode offscreen target
// and reads it back. That leaves score::gfx::Window, ScreenNode and
// MultiWindowNode — i.e. every code path that only exists because there is a
// real platform surface with a swap chain in front of it — unexercised.
//
// These rigs build the same score::gfx::Graph the app builds, but terminate it
// on a ScreenNode / MultiWindowNode instead. They need a real windowing system
// (score_add_test(... GUI) already labels such tests "gui" and does not force
// the offscreen QPA), and they SKIP cleanly when no surface can be created.

#include <score_test/Gfx.hpp>

#include <Gfx/Graph/Graph.hpp>
#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/MultiWindowNode.hpp>
#include <Gfx/Graph/ScreenNode.hpp>
#include <Gfx/Graph/Window.hpp>

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QScreen>
#include <QThread>

#include <catch2/catch_tostring.hpp>

#include <memory>
#include <string>
#include <vector>

namespace Catch
{
// Without these, every geometry assertion below stringifies as "{?}" and a
// failing CHECK tells you nothing about the values involved.
template <>
struct StringMaker<QSize>
{
  static std::string convert(const QSize& s)
  {
    return std::to_string(s.width()) + "x" + std::to_string(s.height());
  }
};
template <>
struct StringMaker<QPoint>
{
  static std::string convert(const QPoint& p)
  {
    return "(" + std::to_string(p.x()) + ", " + std::to_string(p.y()) + ")";
  }
};
template <>
struct StringMaker<QPointF>
{
  static std::string convert(const QPointF& p)
  {
    return "(" + std::to_string(p.x()) + ", " + std::to_string(p.y()) + ")";
  }
};
template <>
struct StringMaker<QRectF>
{
  static std::string convert(const QRectF& r)
  {
    return "[" + std::to_string(r.x()) + ", " + std::to_string(r.y()) + " "
           + std::to_string(r.width()) + "x" + std::to_string(r.height()) + "]";
  }
};
template <>
struct StringMaker<QString>
{
  static std::string convert(const QString& s) { return s.toStdString(); }
};
}

namespace score::test::gfx
{

/// Spin the event loop until `pred` holds or `timeoutMs` elapses.
/// Returns the final value of `pred`.
template <typename P>
inline bool pump_until(P&& pred, int timeoutMs = 4000)
{
  QElapsedTimer t;
  t.start();
  do
  {
    if(pred())
      return true;
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    QCoreApplication::sendPostedEvents();
    QThread::msleep(2);
  } while(t.elapsed() < timeoutMs);
  return pred();
}

/// Spin the event loop for `ms` without any completion condition.
inline void pump_for(int ms)
{
  QElapsedTimer t;
  t.start();
  do
  {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    QCoreApplication::sendPostedEvents();
    QThread::msleep(2);
  } while(t.elapsed() < ms);
}

/// True when the platform can actually give us a mapped, exposed native
/// window. Cached: asking a headless platform repeatedly is pointless and,
/// on the offscreen QPA, GL surface teardown is fragile (see probe_api).
inline bool can_present()
{
  static int cached = -1;
  if(cached >= 0)
    return cached == 1;

  const auto plat = QGuiApplication::platformName();
  if(plat.contains("offscreen") || plat.contains("minimal"))
  {
    cached = 0;
    return false;
  }
  if(QGuiApplication::screens().isEmpty())
  {
    cached = 0;
    return false;
  }

  QWindow w;
  w.setSurfaceType(QSurface::OpenGLSurface);
  w.resize(64, 64);
  w.show();
  const bool ok = pump_until([&] { return w.isExposed(); }, 3000);
  w.destroy();
  cached = ok ? 1 : 0;
  return ok;
}

/// A single-window rig: ISF producer -> ScreenNode.
///
/// Member order matters exactly as in GfxPipeline: `graph` is declared last so
/// it is destroyed first, while the nodes it references are still alive.
struct ScreenRig
{
  ScreenRig() = default;
  ScreenRig(const ScreenRig&) = delete;
  ScreenRig& operator=(const ScreenRig&) = delete;

  /// Build "isf-solid-color.fs -> ScreenNode(size)". Returns false and fills
  /// error()/skipReason() as render_isf_chain does.
  bool build(
      score::gfx::GraphicsApi api, QSize size = {256, 192},
      const QString& shader = QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-solid-color.fs"),
      score::gfx::OutputNode::Configuration conf
      = {.manualRenderingRate = 1000. / 60., .supportsVSync = true})
  {
    m_backend = backend_name(api);

    if(!can_present())
    {
      m_skipped = true;
      m_skipReason = "no windowing system able to expose a native window";
      return false;
    }

    std::string probed;
    if(!probe_api(api, probed))
    {
      m_skipped = true;
      m_skipReason
          = std::string("RHI backend '") + backend_name(api) + "' cannot initialize here";
      return false;
    }

    auto built = make_isf_node(shader);
    if(!built.node)
    {
      m_error = built.error;
      return false;
    }
    src = std::move(built.node);
    src->nodeId = 1;

    screen = std::make_unique<score::gfx::ScreenNode>(conf);
    screen->nodeId = 2;
    screen->setSize(size);
    screen->setTitle(QStringLiteral("score gfx test"));
    screen->onFps = [this](float f) {
      ++fpsCount;
      lastFps = f;
    };

    graph.addNode(src.get());
    graph.addNode(screen.get());
    auto* out = nth_image_output(*src, 0);
    if(!out)
    {
      m_error = "ISF node has no image output";
      return false;
    }
    graph.addEdge(out, screen->input[0], Process::CableType::ImmediateGlutton);

    graph.createAllRenderLists(api);

    if(!pump_until([this] { return screen->canRender(); }, 5000))
    {
      m_skipped = true;
      m_skipReason = "the ScreenNode's swap chain never became ready";
      return false;
    }

    if(auto rs = screen->renderState(); rs && rs->rhi)
      m_backend = rs->rhi->backendName();
    return true;
  }

  /// Pump `frames` graph frames through the window (the manual-rendering path
  /// the app's non-vsync timer drives).
  void render(int frames)
  {
    std::vector<score::gfx::Node*> procs{src.get()};
    std::vector<score::gfx::OutputNode*> sinks{screen.get()};
    for(int f = 0; f < frames; ++f)
    {
      pump_frame(procs, sinks, int(m_frame), frames);
      ++m_frame;
      QCoreApplication::processEvents(QEventLoop::AllEvents, 2);
    }
  }

  score::gfx::Window* window() const noexcept
  {
    return screen ? screen->window().get() : nullptr;
  }

  bool skipped() const noexcept { return m_skipped; }
  const std::string& skipReason() const noexcept { return m_skipReason; }
  const std::string& error() const noexcept { return m_error; }
  const std::string& backend() const noexcept { return m_backend; }

  int fpsCount{};
  float lastFps{-1.f};

  std::unique_ptr<score::gfx::ISFNode> src;
  std::unique_ptr<score::gfx::ScreenNode> screen;
  score::gfx::Graph graph;

private:
  int64_t m_frame{};
  bool m_skipped{};
  std::string m_skipReason;
  std::string m_error;
  std::string m_backend;
};

/// A ScreenNode driven WITHOUT a Graph: createOutput() straight onto the node.
///
/// This is the rig for everything that tears the output down (swapchain flag /
/// format toggles, graphics-API changes). Doing that under a live Graph is a
/// use-after-free — see tests/gfx/ScreenOutputFindings.cpp — so the teardown
/// paths are exercised here, where no RenderList was ever built against the
/// QRhi that destroyOutput() frees.
struct BareScreenRig
{
  BareScreenRig() = default;
  BareScreenRig(const BareScreenRig&) = delete;
  BareScreenRig& operator=(const BareScreenRig&) = delete;

  ~BareScreenRig()
  {
    if(screen)
      screen->destroyOutput();
  }

  bool build(score::gfx::GraphicsApi api, QSize size = {192, 144})
  {
    m_backend = backend_name(api);
    if(!can_present())
    {
      m_skipped = true;
      m_skipReason = "no windowing system able to expose a native window";
      return false;
    }
    std::string probed;
    if(!probe_api(api, probed))
    {
      m_skipped = true;
      m_skipReason
          = std::string("RHI backend '") + backend_name(api) + "' cannot initialize here";
      return false;
    }

    screen = std::make_unique<score::gfx::ScreenNode>(
        score::gfx::OutputNode::Configuration{
            .manualRenderingRate = 1000. / 60., .supportsVSync = true});
    screen->setSize(size);
    screen->createOutput(
        {.graphicsApi = api, .onReady = [this] { ++readyCount; },
         .onResize = [this] { ++resizeCount; }});

    if(!pump_until([this] { return screen->canRender(); }, 5000))
    {
      m_skipped = true;
      m_skipReason = "the ScreenNode's swap chain never became ready";
      return false;
    }
    if(auto rs = screen->renderState(); rs && rs->rhi)
      m_backend = rs->rhi->backendName();
    return true;
  }

  score::gfx::Window* window() const noexcept
  {
    return screen ? screen->window().get() : nullptr;
  }

  bool skipped() const noexcept { return m_skipped; }
  const std::string& skipReason() const noexcept { return m_skipReason; }
  const std::string& error() const noexcept { return m_error; }
  const std::string& backend() const noexcept { return m_backend; }

  int readyCount{};
  int resizeCount{};
  std::unique_ptr<score::gfx::ScreenNode> screen;

private:
  bool m_skipped{};
  std::string m_skipReason;
  std::string m_error;
  std::string m_backend;
};

/// A multi-window rig: ISF producer -> MultiWindowNode(mappings).
struct MultiWindowRig
{
  MultiWindowRig() = default;
  MultiWindowRig(const MultiWindowRig&) = delete;
  MultiWindowRig& operator=(const MultiWindowRig&) = delete;

  bool build(
      score::gfx::GraphicsApi api, std::vector<Gfx::OutputMapping> mappings,
      const QString& shader = QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-gradient-x.fs"))
  {
    m_backend = backend_name(api);

    if(!can_present())
    {
      m_skipped = true;
      m_skipReason = "no windowing system able to expose a native window";
      return false;
    }

    std::string probed;
    if(!probe_api(api, probed))
    {
      m_skipped = true;
      m_skipReason
          = std::string("RHI backend '") + backend_name(api) + "' cannot initialize here";
      return false;
    }

    auto built = make_isf_node(shader);
    if(!built.node)
    {
      m_error = built.error;
      return false;
    }
    src = std::move(built.node);
    src->nodeId = 1;

    node = std::make_unique<score::gfx::MultiWindowNode>(
        score::gfx::OutputNode::Configuration{
            .manualRenderingRate = 1000. / 60., .supportsVSync = false},
        mappings);
    node->nodeId = 2;
    node->onFps = [this](float f) { ++fpsCount; };
    node->onWindowsCreated = [this] { ++windowsCreatedCount; };

    graph.addNode(src.get());
    graph.addNode(node.get());
    auto* out = nth_image_output(*src, 0);
    if(!out)
    {
      m_error = "ISF node has no image output";
      return false;
    }
    graph.addEdge(out, node->input[0], Process::CableType::ImmediateGlutton);

    graph.createAllRenderLists(api);

    if(!node->canRender())
    {
      m_skipped = true;
      m_skipReason = "MultiWindowNode could not create its QRhi / offscreen target";
      return false;
    }

    // Windows come up asynchronously, one swap chain per expose.
    pump_until(
        [this] {
      for(auto& wo : node->windowOutputs())
        if(!wo.hasSwapChain)
          return false;
      return !node->windowOutputs().empty();
        },
        5000);

    if(auto rs = node->renderState(); rs && rs->rhi)
      m_backend = rs->rhi->backendName();
    return true;
  }

  void render(int frames)
  {
    std::vector<score::gfx::Node*> procs{src.get()};
    std::vector<score::gfx::OutputNode*> sinks{node.get()};
    for(int f = 0; f < frames; ++f)
    {
      pump_frame(procs, sinks, int(m_frame), frames);
      ++m_frame;
      QCoreApplication::processEvents(QEventLoop::AllEvents, 2);
    }
  }

  bool skipped() const noexcept { return m_skipped; }
  const std::string& skipReason() const noexcept { return m_skipReason; }
  const std::string& error() const noexcept { return m_error; }
  const std::string& backend() const noexcept { return m_backend; }

  int fpsCount{};
  int windowsCreatedCount{};

  std::unique_ptr<score::gfx::ISFNode> src;
  std::unique_ptr<score::gfx::MultiWindowNode> node;
  score::gfx::Graph graph;

private:
  int64_t m_frame{};
  bool m_skipped{};
  std::string m_skipReason;
  std::string m_error;
  std::string m_backend;
};

}
