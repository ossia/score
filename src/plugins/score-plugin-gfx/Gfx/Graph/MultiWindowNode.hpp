#pragma once
#include <Gfx/Graph/FloatImageIO.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/OutputQuality.hpp>
#include <Gfx/Graph/Utils.hpp>
#include <Gfx/WindowDevice.hpp>

namespace score::gfx
{
class Window;

struct SCORE_PLUGIN_GFX_TEST_EXPORT MultiWindowNode : OutputNode
{
  explicit MultiWindowNode(
      Configuration conf, const std::vector<Gfx::OutputMapping>& mappings,
      QSize canvasSize = QSize{1280, 720});
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

    std::shared_ptr<QRhiReadbackResult> readback{std::make_shared<QRhiReadbackResult>()};
    int readbackRequests{};

    Gfx::OutputColor color;
    Gfx::OutputQuality quality;
    std::shared_ptr<const CubeLut> lut;
    std::shared_ptr<const FloatImage> warpMap;
    //! 99.5th percentile of the change in source u and v between valid
    //! warp-map texels: u, v along a row, then u, v along a column.
    std::array<float, 4> warpStep{};
    std::shared_ptr<const FloatImage> alphaMap;
    std::shared_ptr<const FloatImage> betaMap;
    //! size x 1, rgb = projector code for linear light i / (size - 1).
    std::shared_ptr<const FloatImage> responseMap;
    //! Rows of the compensation matrix, rgba = (M_k0, M_k1, M_k2, o_k).
    std::array<std::shared_ptr<const FloatImage>, 3> compensationMaps;

    //! Bumped when the texture of a slot changes: MapSlot order, then the LUT.
    std::array<int, 8> mapVersions{};
    //! What each map field was loaded from: path, modification time and size.
    std::array<QString, 6> mapStamps;
    bool responseWithMixing{};
  };

  enum MapSlot
  {
    WarpSlot,
    AlphaSlot,
    BetaSlot,
    ResponseSlot,
    Comp0Slot,
    Comp1Slot,
    Comp2Slot,
    MapSlotCount,
    LutSlot = MapSlotCount
  };

  const std::vector<WindowOutput>& windowOutputs() const noexcept
  {
    return m_windowOutputs;
  }
  //! The mappings the outputs are (re)built from, live changes included.
  const std::vector<Gfx::OutputMapping>& mappings() const noexcept { return m_mappings; }

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
  void setColor(int windowIndex, const Gfx::OutputColor& color);
  //! Arms a readback of the window's next presented frame into windowOutputs()[i].readback.
  //! Needs setReadbackSupport(true) before createOutput().
  void requestReadback(int windowIndex);
  void setReadbackSupport(bool b) noexcept { m_readbackSupport = b; }
  void setMaps(int windowIndex, const Gfx::OutputMaps& maps);
  void setQuality(int windowIndex, const Gfx::OutputQuality& quality);

  //! What the per-window blit does with a window's settings.
  struct EffectiveQuality
  {
    //! 0 linear, 1 trilinear, 2 anisotropic.
    int sampling{};
    //! Quantization levels the dither targets (255, 1023), 0 = off.
    int ditherLevels{};
    //! Encoding of the canvas for blending in linear light: 0 = gamma 2.2,
    //! 1 = linear (scRGB / linear P3 swap chains), 2 = SMPTE ST 2084 (HDR10).
    int transfer{};
  };
  EffectiveQuality effectiveQuality(const WindowOutput& wo) const noexcept;
  //! Whether the warp map samples the canvas at more than a texel per output pixel.
  bool warpMinifies(const WindowOutput& wo) const noexcept;
  //! The canvas format and whether it needs mips, for the current settings.
  std::pair<QRhiTexture::Format, bool> canvasSpec() const noexcept;

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
  //! Queues a canvas rebuild when canvasSpec() changed.
  void updateCanvasSpec();
  //! canvasSpec() that never drops precision or mips while the output lives:
  //! a flip rebuilds the whole render list.
  std::pair<QRhiTexture::Format, bool> upgradedCanvas() const noexcept;
  //! Reloads the fields whose path or file changed; a failed reload of the
  //! same path keeps the map it had.
  void loadMaps(int windowIndex, const Gfx::OutputMaps& maps);
  bool m_canvasMips{};
  bool m_canvasUpdateQueued{};
  bool m_readbackSupport{};

  Configuration m_conf;
  std::vector<Gfx::OutputMapping> m_mappings;
  QSize m_canvasSize;
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
