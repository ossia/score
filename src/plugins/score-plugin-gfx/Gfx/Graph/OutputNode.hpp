#pragma once
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/Uniforms.hpp>

#include <score_plugin_gfx_export.h>

#include <memory>

namespace score::gfx
{
class GpuResourceRegistry;
struct OutputConfiguration
{
  GraphicsApi graphicsApi{};
  std::function<void()> onReady;
  std::function<void()> onResize;
};

class SCORE_PLUGIN_GFX_EXPORT OutputNodeRenderer : public score::gfx::NodeRenderer
{
public:
  using score::gfx::NodeRenderer::NodeRenderer;
  virtual ~OutputNodeRenderer();
  virtual void
  finishFrame(RenderList&, QRhiCommandBuffer& commands, QRhiResourceUpdateBatch*& res);

  // Sinks have no output edges, so there is nothing to release per-edge.
  // Concrete sinks may still override (e.g. to drop per-input bookkeeping
  // routed through addOutputPass), but the default is a true no-op rather
  // than the dangerous silent base-class no-op.
  void removeOutputPass(RenderList&, Edge&) override { }
};

class Window;
/**
 * @brief Base class for sink nodes (QWindow, spout, syphon, NDI output, ...)
 */
class SCORE_PLUGIN_GFX_EXPORT OutputNode : public score::gfx::Node
{
public:
  virtual ~OutputNode();

  virtual void setRenderer(std::shared_ptr<RenderList>) = 0;
  virtual RenderList* renderer() const = 0;

  OutputNodeRenderer* createRenderer(RenderList& r) const noexcept override = 0;

  virtual void startRendering() = 0;
  virtual void render() = 0;
  virtual void stopRendering() = 0;
  virtual bool canRender() const = 0;
  virtual void onRendererChange() = 0;

  /**
   * @brief Set a callback to drive rendering when there is a single output.
   *
   * If we render to a single window, we can use the GPU V-Sync mechanism.
   * Otherwise the implementation will create timers to keep things in sync.
   */
  virtual void setVSyncCallback(std::function<void()>);

  virtual void createOutput(OutputConfiguration conf) = 0;

  virtual void updateGraphicsAPI(GraphicsApi);
  virtual void destroyOutput() = 0;
  virtual std::shared_ptr<RenderState> renderState() const = 0;

  struct Configuration
  {
    // If set, the host is responsible for calling render() at this
    // rate (given in milliseconds)
    std::optional<double> manualRenderingRate;
    bool outputNeedsRenderPass{};
    bool supportsVSync{};
    OutputNode* parent{};
  };

  virtual Configuration configuration() const noexcept = 0;

  /**
   * @brief Persistent GPU resource registry for this output.
   *
   * Persist-across-rebuild contract: this used to live on the
   * RenderList (created in RenderList::init, destroyed in
   * RenderList::release), so every viewport-resize-driven RL rebuild
   * threw away ~100 MiB of texture-array data, the mesh slabs, and
   * the producer arena slot indices — all of which describe scene
   * content, not framebuffer state. Hoisting ownership to the
   * OutputNode lets these survive across `Graph::recreateOutputRenderList`.
   *
   * Lifetime: lazy-allocated on first acquireRegistry() call (typically
   * from RenderList::init), tied to the OutputNode's QRhi. Concrete
   * outputs MUST call releaseRegistry() inside their destroyOutput()
   * BEFORE tearing down the QRhi (via RenderState::destroy or
   * setSwapchainFormat-style replacement) — otherwise the registry's
   * QRhi resources would be freed against a destroyed device.
   *
   * Returns a non-null reference. Always allocates if the slot is empty.
   */
  GpuResourceRegistry& acquireRegistry();

  /**
   * @brief Non-owning accessor. Returns null if no registry has been
   * acquired yet (e.g. queried before the first RenderList::init).
   */
  GpuResourceRegistry* registry() const noexcept { return m_registry.get(); }

  /**
   * @brief Tear down the registry's QRhi resources directly. Idempotent.
   *
   * MUST be called by concrete subclasses' destroyOutput() before they
   * tear down the QRhi. Calls GpuResourceRegistry::destroyOwned() which
   * `delete`s the buffer / texture / sampler wrappers (the QRhi is
   * still alive at that point — the caller's responsibility), then
   * resets the unique_ptr so a subsequent acquireRegistry() rebuilds
   * fresh against the new QRhi.
   *
   * Safe to call when no registry exists (no-op).
   */
  void releaseRegistry();

protected:
  explicit OutputNode();

  // Persistent across RenderList rebuilds. See acquireRegistry() docs.
  // unique_ptr is opaque-typed in this header (forward-declared above);
  // its destructor needs the full type, hence the out-of-line ~OutputNode
  // implementation in OutputNode.cpp.
  std::unique_ptr<GpuResourceRegistry> m_registry;
};
}
