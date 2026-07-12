#pragma once

/**
 * @file DirectVideoOutputNode.hpp
 * @brief Vendor-neutral OutputNode for professional capture-card playout.
 *
 * Owns the QRhi + render loop + the createOutput orchestration; all device
 * specifics come from a DirectVideoOutputBackend (AJA today; DeckLink/Bluefish/
 * Deltacast next). The node composes the already-generic pieces around the
 * backend: makeWireEncoder(encoderFormat), selectVideoOutputStrategy(gpuDirect
 * Candidates), CpuStagedVideoOutput(planes/registrar/customStage), PacedFramePump(
 * pacingHooks).
 *
 * A vendor output node becomes a thin subclass that constructs the node with
 * its backend, e.g. `AJANode : DirectVideoOutputNode` passing an AjaOutputBackend.
 */

#include <Gfx/Graph/DirectVideoOutputBackend.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/interop/GpuCapabilities.hpp>

#include <score_plugin_gfx_export.h>

#include <atomic>
#include <cstdint>
#include <memory>

namespace score::gfx
{
class RenderList;
struct RenderState;
namespace interop
{
class CpuStagedVideoOutput;
class PacedFramePump;
}

class SCORE_PLUGIN_GFX_EXPORT DirectVideoOutputNode : public OutputNode
{
public:
  explicit DirectVideoOutputNode(std::unique_ptr<DirectVideoOutputBackend> backend);
  ~DirectVideoOutputNode() override;

  // OutputNode interface
  void setRenderer(std::shared_ptr<RenderList> r) override;
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

  /// Name of the engaged GPU-direct strategy, or "cpu-staging".
  const char* activeStrategyName() const noexcept;

  /// Pacing diagnostics (0 if the pump isn't running). Useful for harnesses.
  std::uint64_t pacingGoodXfers() const noexcept;
  std::uint64_t pacingDrops() const noexcept;
  std::uint64_t pacingUnderruns() const noexcept;

protected:
  std::unique_ptr<DirectVideoOutputBackend> m_backend;

  std::shared_ptr<RenderState> m_renderState;
  std::weak_ptr<RenderList> m_renderer;
  QRhi* m_rhi{};
  QRhiTexture* m_texture{};
  QRhiTextureRenderTarget* m_renderTarget{};

  std::unique_ptr<interop::CpuStagedVideoOutput> m_hostStaged;
  std::unique_ptr<interop::VideoOutputStrategy> m_rdma;
  std::unique_ptr<interop::PacedFramePump> m_pump;

  /// GPU interop probe, borrowed by CpuStagedVideoOutput's DVP HostPinnedRing; must
  /// outlive m_hostStaged, hence a node member.
  interop::GpuCapabilities m_caps{};

  std::atomic<bool> m_running{false};
  GraphicsApi m_graphicsApi{};
};

} // namespace score::gfx
