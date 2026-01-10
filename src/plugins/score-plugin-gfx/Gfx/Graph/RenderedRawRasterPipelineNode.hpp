#pragma once

#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderedISFUtils.hpp>

namespace score::gfx
{
// Used for the simple case of a single, non-persistent pass (the most common case)

struct RenderedRawRasterPipelineNode : score::gfx::NodeRenderer
{
  explicit RenderedRawRasterPipelineNode(const ISFNode& node) noexcept;

  virtual ~RenderedRawRasterPipelineNode();

  TextureRenderTarget renderTargetForInput(const Port& p) override;

  void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void update(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge) override;
  bool updateMaterials(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge);
  void release(RenderList& r) override;

  void runInitialPasses(
      RenderList&, QRhiCommandBuffer& commands, QRhiResourceUpdateBatch*& res,
      Edge& edge) override;

  void runRenderPass(RenderList&, QRhiCommandBuffer& commands, Edge& edge) override;

  void process(int32_t port, const ossia::transform3d& v) override;

private:
  ossia::small_flat_map<const Port*, TextureRenderTarget, 2> m_rts;

  void initPass(const TextureRenderTarget& rt, RenderList& renderer, Edge& edge);

  std::vector<Sampler> allSamplers() const noexcept;

  ossia::small_vector<std::pair<Edge*, Pass>, 2> m_passes;

  ISFNode& n;

  std::vector<Sampler> m_inputSamplers;
  std::vector<Sampler> m_audioSamplers;

  int64_t meshChangedIndex{-1};
  const Mesh* m_mesh{};
  MeshBuffers m_meshbufs;

  QRhiBuffer* m_materialUBO{};
  int m_materialSize{};

  QRhiBuffer* m_modelUBO{};

  std::optional<AudioTextureUpload> m_audioTex;

  // The part of the m_materialUBO for which changes
  // trigger a pipeline recreation (blend status etc.)
  static constexpr int size_of_pipeline_material = 32;
  char m_prevPipelineChangingMaterial[size_of_pipeline_material]{0};
  struct PipelineChangingMaterial
  {
    int32_t mode;         // tri, point, line
    int32_t enable_blend; // bool
    QRhiGraphicsPipeline::BlendFactor src_color;
    QRhiGraphicsPipeline::BlendFactor dst_color;
    QRhiGraphicsPipeline::BlendOp op_color;
    QRhiGraphicsPipeline::BlendFactor src_alpha;
    QRhiGraphicsPipeline::BlendFactor dst_alpha;
    QRhiGraphicsPipeline::BlendOp op_alpha;
  };
  static_assert(sizeof(PipelineChangingMaterial) == size_of_pipeline_material);

  ossia::transform3d m_modelTransform;
};
}
