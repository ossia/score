#pragma once
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/WindowDevice.hpp>

namespace score::gfx
{
class Window;

struct SCORE_PLUGIN_GFX_EXPORT MultiWindowNode : OutputNode
{
  explicit MultiWindowNode(
      Configuration conf, const std::vector<Gfx::OutputMapping>& mappings);
  virtual ~MultiWindowNode();

  void startRendering() override;
  void render() override;
  void onRendererChange() override;
  bool canRender() const override;
  void stopRendering() override;

  void setRenderer(std::shared_ptr<RenderList> r) override;
  RenderList* renderer() const override;

  void createOutput(score::gfx::OutputConfiguration) override;
  void destroyOutput() override;
  void updateGraphicsAPI(GraphicsApi) override;
  void setVSyncCallback(std::function<void()>) override;

  std::shared_ptr<RenderState> renderState() const override;
  score::gfx::OutputNodeRenderer* createRenderer(RenderList& r) const noexcept override;
  Configuration configuration() const noexcept override;

  struct WindowOutput
  {
    std::shared_ptr<Window> window;
    QRhiSwapChain* swapChain{};
    QRhiRenderBuffer* depthStencil{};
    QRhiRenderPassDescriptor* renderPassDescriptor{};
    QRectF sourceRect{0, 0, 1, 1};
    bool hasSwapChain{};
  };

  const std::vector<WindowOutput>& windowOutputs() const noexcept
  {
    return m_windowOutputs;
  }

  void setRenderSize(QSize sz);
  void setSourceRect(int windowIndex, QRectF rect);

  std::function<void(float)> onFps;
  // Called after all windows are created in createOutput(), before swap chains are initialized
  std::function<void()> onWindowsCreated;

private:
  void renderBlack();
  void initWindow(int index, GraphicsApi api);

  Configuration m_conf;
  std::vector<Gfx::OutputMapping> m_mappings;
  std::vector<WindowOutput> m_windowOutputs;

  std::shared_ptr<RenderState> m_renderState;
  std::weak_ptr<RenderList> m_renderer;

  std::function<void()> m_vsyncCallback;
};
}
