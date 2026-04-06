#pragma once
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/Utils.hpp>
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

  struct EdgeBlendData
  {
    float width{0.0f};
    float gamma{2.2f};
  };

  struct WindowOutput
  {
    std::shared_ptr<Window> window;
    QRhiSwapChain* swapChain{};
    QRhiRenderBuffer* depthStencil{};
    QRhiRenderPassDescriptor* renderPassDescriptor{};
    QRectF sourceRect{0, 0, 1, 1};
    bool hasSwapChain{};

    EdgeBlendData blendLeft, blendRight, blendTop, blendBottom;

    Gfx::CornerWarp cornerWarp;

    int rotation{0};
    bool mirrorX{false};
    bool mirrorY{false};
  };

  const std::vector<WindowOutput>& windowOutputs() const noexcept
  {
    return m_windowOutputs;
  }

  // Stable offscreen render target used as the input target for the upstream
  // graph. Owned by the node so it exists independently of any window being
  // ready. Every upstream pipeline renders into this target; the per-window
  // blit pipelines sample from it.
  const TextureRenderTarget& offscreenTarget() const noexcept
  {
    return m_offscreenTarget;
  }

  void setRenderSize(QSize sz);
  void setSourceRect(int windowIndex, QRectF rect);
  void setEdgeBlend(int windowIndex, int side, float width, float gamma);
  void setCornerWarp(int windowIndex, const Gfx::CornerWarp& warp);
  void setTransform(int windowIndex, int rotation, bool mirrorX, bool mirrorY);
  void setSwapchainFlag(Gfx::SwapchainFlag flag);
  void setSwapchainFormat(Gfx::SwapchainFormat format);

  std::function<void(float)> onFps;
  // Called once all windows have been constructed in createOutput(),
  // before any of them has a swap chain. Used by multiwindow_device to
  // connect Qt signals.
  std::function<void()> onWindowsCreated;

private:
  void renderBlack();
  // Per-window swap chain init / release. Idempotent and safe to call
  // multiple times (e.g. on window re-expose after a close).
  void initWindowSwapChain(int index);
  void releaseWindowSwapChain(int index);
  // (Re)creates the offscreen render target using the current
  // renderSize / renderFormat / samples in m_renderState. The
  // offscreen RPD is also assigned to m_renderState->renderPassDescriptor.
  void recreateOffscreenTarget();

  Configuration m_conf;
  std::vector<Gfx::OutputMapping> m_mappings;
  std::vector<WindowOutput> m_windowOutputs;

  std::shared_ptr<RenderState> m_renderState;
  std::weak_ptr<RenderList> m_renderer;

  TextureRenderTarget m_offscreenTarget;

  Gfx::SwapchainFlag m_swapchainFlag{};
  Gfx::SwapchainFormat m_swapchainFormat{};
  std::function<void()> m_vsyncCallback;
  // Stored resize callback from createOutput() so per-window events can
  // request a full render-list rebuild (e.g. when a window becomes ready
  // after the render list has already been built).
  std::function<void()> m_onResize;
};
}
