#pragma once

#include <Gfx/Graph/RenderState.hpp>
#include <verdigris>
#include <QWindow>
namespace score::gfx
{
struct ScreenNode;

/**
 * @brief A platform window in which the content is going to be rendered
 *
 * Created by ScreenNode.
 */
class Window : public QWindow
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

  RenderState state;

  void mouseMove(QPointF screen, QPointF win)
      W_SIGNAL(mouseMove, screen, win);
  void key(int key, const QString& t)
      W_SIGNAL(key, key, t);
private:
  GraphicsApi m_api{};
  QRhiSwapChain* m_swapChain{};

  bool m_canRender = false;
  bool m_running = false;
  bool m_notExposed = false;
  bool m_newlyExposed = false;
  bool m_hasSwapChain = false;
};
}
