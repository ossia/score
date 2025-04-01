#pragma once
#include <Gfx/Graph/OutputNode.hpp>

class QScreen;
class QTabletEvent;
namespace score::gfx
{
/**
 * @brief This node is used for rendering to a score::gfx::Window.
 */
struct SCORE_PLUGIN_GFX_EXPORT ScreenNode : OutputNode
{
  ScreenNode(bool embedded = false, bool startFullScreen = false);
  virtual ~ScreenNode();

  void startRendering() override;
  void render() override;
  void onRendererChange() override;
  bool canRender() const override;
  void stopRendering() override;

  void setRenderer(std::shared_ptr<RenderList> r) override;
  RenderList* renderer() const override;

  void setScreen(QScreen*);
  void setPosition(QPoint pos);
  void setSize(QSize sz);
  void setRenderSize(QSize sz);
  void setFullScreen(bool);

  void createOutput(
      GraphicsApi graphicsApi, std::function<void()> onReady,
      std::function<void()> onUpdate, std::function<void()> onResize) override;
  void destroyOutput() override;
  void updateGraphicsAPI(GraphicsApi) override;

  std::shared_ptr<RenderState> renderState() const override;
  score::gfx::OutputNodeRenderer* createRenderer(RenderList& r) const noexcept override;
  Configuration configuration() const noexcept override;

  const std::shared_ptr<Window>& window() const noexcept { return m_window; }

  std::function<void(QPointF, QPointF)> onMouseMove;
  std::function<void(QTabletEvent*)> onTabletMove;
  std::function<void(int, const QString&)> onKey;

private:
  class BasicRenderer;
  class ScaledRenderer;
  std::shared_ptr<Window> m_window{};
  QRhiSwapChain* m_swapChain{};
  QRhiRenderBuffer* m_depthStencil{};
  QScreen* m_screen{};
  std::optional<QPoint> m_pos{};
  std::optional<QSize> m_sz{};
  std::optional<QSize> m_renderSz{};

  bool m_embedded{};
  bool m_fullScreen{};
  bool m_ownsWindow{};
};
}
