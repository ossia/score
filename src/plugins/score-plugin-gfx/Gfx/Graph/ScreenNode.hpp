#pragma once
#include <Gfx/Graph/OutputNode.hpp>

namespace score::gfx
{
/**
 * @brief This node is used for rendering to a score::gfx::Window.
 */
struct SCORE_PLUGIN_GFX_EXPORT ScreenNode : OutputNode
{
  ScreenNode(bool embedded = false, bool startFullScreen = false);
  ScreenNode(std::shared_ptr<Window>);
  virtual ~ScreenNode();

  void startRendering() override;
  void onRendererChange() override;
  bool canRender() const override;
  void stopRendering() override;

  void setRenderer(RenderList* r) override;
  RenderList* renderer() const override;

  void createOutput(
      GraphicsApi graphicsApi,
      std::function<void()> onReady,
      std::function<void()> onUpdate,
      std::function<void()> onResize) override;
  void destroyOutput() override;
  void updateGraphicsAPI(GraphicsApi) override;

  RenderState* renderState() const override;
  score::gfx::NodeRenderer*
  createRenderer(RenderList& r) const noexcept override;

  const std::shared_ptr<Window>& window() const noexcept { return m_window; }

private:
  class Renderer;
  std::shared_ptr<Window> m_window{};
  QRhiSwapChain* m_swapChain{};

  bool m_embedded{};
  bool m_fullScreen{};
  bool m_ownsWindow{};
};
}
