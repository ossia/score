#pragma once

#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderedISFUtils.hpp>

namespace score::gfx
{
struct SimpleRenderedVSANode : score::gfx::NodeRenderer
{
  explicit SimpleRenderedVSANode(const ISFNode& node) noexcept;

  virtual ~SimpleRenderedVSANode();

  TextureRenderTarget renderTargetForInput(const Port& p) override;

  void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void update(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge) override;
  void release(RenderList& r) override;

  void runInitialPasses(
      RenderList&, QRhiCommandBuffer& commands, QRhiResourceUpdateBatch*& res,
      Edge& edge) override;

  void runRenderPass(RenderList&, QRhiCommandBuffer& commands, Edge& edge) override;

private:
  ossia::small_flat_map<const Port*, TextureRenderTarget, 2> m_rts;

  void initPass(
      const TextureRenderTarget& rt, RenderList& renderer, Edge& edge,
      QRhiResourceUpdateBatch& res);

  std::vector<Sampler> allSamplers() const noexcept;

  struct PassData
  {
    Edge* edge;
    Pass main_pass;
    QRhiGraphicsPipeline* background_pipeline{};
    QRhiShaderResourceBindings* background_srb{};
    QRhiBuffer* background_ubo{};
    MeshBuffers background_tri{};
  };

  ossia::small_vector<PassData, 2> m_passes;

  ISFNode& n;

  std::vector<Sampler> m_inputSamplers;
  std::vector<Sampler> m_audioSamplers;

  Mesh* m_mesh{};

  QRhiBuffer* m_materialUBO{};
  int m_materialSize{};

  std::optional<AudioTextureUpload> m_audioTex;

  int m_prevFormat{};
};
}
