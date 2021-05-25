#pragma once

#include <Gfx/Graph/RenderState.hpp>

#include <QWindow>
namespace score::gfx
{
struct ScreenNode;

/**
 * @brief A platform window in which the content is going to be rendered
 */
class Window : public QWindow
{
  friend ScreenNode;

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
