#pragma once

#include <Gfx/Graph/RenderState.hpp>

#include <score/tools/Events.hpp>

#include <QElapsedTimer>
#include <QTabletEvent>
#include <QWindow>

#include <score_plugin_gfx_export.h>

#include <verdigris>
namespace score::gfx
{
struct ScreenNode;

/**
 * @brief A platform window in which the content is going to be rendered
 *
 * Created by ScreenNode.
 */
class SCORE_PLUGIN_GFX_EXPORT Window : public QWindow
{
  friend ScreenNode;
  W_OBJECT(Window)

public:
  explicit Window(GraphicsApi graphicsApi);
  ~Window();

  GraphicsApi api() const noexcept { return m_api; }

  void init();

  void resizeSwapChain();
  void releaseSwapChain();
  void render();

  void exposeEvent(QExposeEvent*) override;
  void mouseDoubleClickEvent(QMouseEvent* ev) override;

  bool event(QEvent* e) override;

  std::function<void()> onWindowReady;
  std::function<void()> onUpdate;
  std::function<void(QRhiCommandBuffer&)> onRender;
  std::function<void()> onResize;

  void tabletMove(QTabletEvent* ev) W_SIGNAL(tabletMove, ev);

  void mouseMove(QPointF screen, QPointF win) W_SIGNAL(mouseMove, screen, win);

  void interactiveEvent(QEvent* event)
      E_SIGNAL(SCORE_PLUGIN_GFX_EXPORT, interactiveEvent, event);

  void key(int key, const QString& t) W_SIGNAL(key, key, t);
  void keyRelease(int key, const QString& t) W_SIGNAL(keyRelease, key, t);

  void fps(float v) W_SIGNAL(fps, v);

private:
  std::shared_ptr<RenderState> state;
  GraphicsApi m_api{};
  QRhiSwapChain* m_swapChain{};
  QElapsedTimer m_timer;
  QElapsedTimer m_fpsPushTimer;
  double m_fps{0.};

  bool m_closed = false;
  bool m_canRender = false;
  bool m_running = false;
  bool m_notExposed = false;
  bool m_newlyExposed = false;
  bool m_hasSwapChain = false;
  bool m_embeddedFullscreen = false;
};
}

Q_DECLARE_METATYPE(score::gfx::Window*)
W_REGISTER_ARGTYPE(score::gfx::Window*)
