#pragma once

#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderedISFUtils.hpp>

namespace score::gfx
{
// Used for the simple case of a single, non-persistent pass (the most common case)

struct SimpleRenderedISFNode : score::gfx::NodeRenderer
{
  SimpleRenderedISFNode(const ISFNode& node) noexcept;

  virtual ~SimpleRenderedISFNode();

  void updateInputTexture(const Port& input, QRhiTexture* tex) override;
  QRhiTexture* textureForOutput(const Port& output) override;

  void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void update(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge) override;
  void release(RenderList& r) override;

  void runInitialPasses(
      RenderList&, QRhiCommandBuffer& commands, QRhiResourceUpdateBatch*& res,
      Edge& edge) override;

  void runRenderPass(RenderList&, QRhiCommandBuffer& commands, Edge& edge) override;

private:
  void initPass(const TextureRenderTarget& rt, RenderList& renderer, Edge& edge);
  void initMRTPass(RenderList& renderer, QRhiResourceUpdateBatch& res);
  void initMRTBlitPasses(RenderList& renderer, QRhiResourceUpdateBatch& res);

  std::vector<Sampler> allSamplers() const noexcept;

  ossia::small_vector<std::pair<Edge*, Pass>, 2> m_passes;

  ISFNode& n;

  std::vector<Sampler> m_inputSamplers;
  std::vector<Sampler> m_audioSamplers;

  const Mesh* m_mesh{};
  MeshBuffers m_meshBuffer{};

  QRhiBuffer* m_materialUBO{};
  int m_materialSize{};

  std::optional<AudioTextureUpload> m_audioTex;

  // MRT: internally-owned render target with multiple attachments
  TextureRenderTarget m_mrtRenderTarget;
  bool m_hasMRT{false};
  bool m_mrtRenderedThisFrame{false};
};
}
