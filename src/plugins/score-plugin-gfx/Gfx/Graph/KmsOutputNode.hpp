#pragma once

/**
 * @file KmsOutputNode.hpp
 * @brief Direct KMS scanout: the graph renders into the buffer the display
 *        engine reads, with nothing in between.
 *
 * Every other output in this tree ends with a copy. ScreenNode renders the graph
 * into an offscreen target and then blits it into the swapchain image
 * (ScaledRenderer::finishFrame) -- on GLES that pass does not even flip Y, so it
 * is a literal full-frame copy, and at capture resolution it costs about as much
 * as the rest of the passthrough put together. This has no swapchain, so that
 * copy does not exist to be removed.
 *
 * How:
 *   once   N scanout buffers, each  GBM bo -> dma-buf -> EGLImage -> GL texture
 *          -> QRhiTexture::createFrom -> QRhiTextureRenderTarget, and the same
 *          dma-buf -> drmModeAddFB2WithModifiers -> a KMS framebuffer id.
 *   frame  pick a slot whose last flip has completed, hand its render target to
 *          the graph, render, then atomically flip to its framebuffer id.
 *
 * The modifier is the part that decides whether this is actually zero-copy.
 * KmsDevice reports what each plane advertises (PlaneInfo::formats[].modifiers);
 * that list is intersected with what GBM will allocate. Allocate a modifier the
 * plane does not advertise and either the kernel refuses the framebuffer, or --
 * worse -- something inserts a detiling copy where nobody is looking for it.
 *
 * The objection recorded against doing this for the PipeWire output
 * (EglDmaBufExport.hpp: "would need QRhiTextureRenderTarget rebuild per buffer
 * dequeue, which fights QRhi's render-target caching") does not apply here. That
 * is a problem when the consumer hands you a different buffer every frame; we own
 * a fixed set, so the N render targets are built once and rotated. All of them
 * share one render-pass descriptor, so pipelines built against one are valid for
 * the others.
 *
 * Requires DRM master, which a running compositor holds. On an appliance that is
 * fine. Taking one output while a desktop session keeps another needs a DRM
 * lease, which KmsDevice does not implement yet.
 */

#include <Gfx/Graph/OutputNode.hpp>

#include <score_plugin_gfx_export.h>

#include <cstdint>
#include <memory>
#include <string>

namespace score::gfx
{

struct KmsOutputSettings
{
  /// Card node. Empty picks the first one with a connected connector.
  std::string device;

  /// 0 picks the first connected connector.
  std::uint32_t connectorId{0};

  /// 0x0 picks the connector's preferred mode.
  std::uint32_t width{0}, height{0};
  double refreshRate{0.0};

  /// Flip at the next scanline instead of the next vblank: lowest latency, and
  /// it tears. Off by default -- tearing is a choice, not a default.
  bool tearing{false};

  /// Scanout buffers. Three lets one be scanned out, one be queued and one be
  /// rendered into; two makes the renderer wait on every flip.
  std::size_t bufferCount{3};

  /// Log the device, connector, mode and the modifier actually negotiated. On by
  /// default: a silent fallback to a copy is invisible otherwise.
  bool verbose{true};
};

/**
 * @brief OutputNode scanning out directly through DRM/KMS.
 */
class SCORE_PLUGIN_GFX_EXPORT KmsOutputNode : public score::gfx::OutputNode
{
public:
  explicit KmsOutputNode(KmsOutputSettings settings);
  ~KmsOutputNode() override;

  void setRenderer(std::shared_ptr<RenderList>) override;
  RenderList* renderer() const override;

  OutputNodeRenderer* createRenderer(RenderList& r) const noexcept override;

  void startRendering() override;
  void render() override;
  void stopRendering() override;
  bool canRender() const override;
  void onRendererChange() override;

  void createOutput(OutputConfiguration conf) override;
  void destroyOutput() override;
  std::shared_ptr<RenderState> renderState() const override;

  Configuration configuration() const noexcept override;

  /// The slot the graph must render into for the frame being built. Rotates,
  /// which is exactly what this hook exists for.
  TextureRenderTarget currentRenderTarget() const noexcept override;

  /// What the device actually gave us, for harnesses and the settings UI.
  std::string engagedDescription() const;

  struct Impl;

private:
  std::unique_ptr<Impl> d;
};

}
