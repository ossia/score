#pragma once
#include <Gfx/Graph/NodeRenderer.hpp>

namespace score::gfx
{
class GeometryFilterNode;
struct SCORE_PLUGIN_GFX_EXPORT GeometryFilterNodeRenderer : score::gfx::NodeRenderer
{
  explicit GeometryFilterNodeRenderer(const GeometryFilterNode& node) noexcept;
  virtual ~GeometryFilterNodeRenderer();

  TextureRenderTarget renderTargetForInput(const Port& p) override;
  void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void update(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge) override;
  void release(RenderList& r) override;

  void runInitialPasses(
      RenderList&, QRhiCommandBuffer& commands, QRhiResourceUpdateBatch*& res,
      Edge& edge) override;

  void runRenderPass(RenderList&, QRhiCommandBuffer& commands, Edge& edge) override;

  QRhiBuffer* material() const noexcept { return m_materialUBO; }

private:
  GeometryFilterNode& node() const noexcept;

  ossia::geometry_spec outputGeometry;

  QRhiBuffer* m_materialUBO{};
  int m_materialSize{};
  int m_dirtyTransformIndex{-1};
};

}
