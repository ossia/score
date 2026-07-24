#pragma once

#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/IsfBindingsBuilder.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderedISFUtils.hpp>

#include <ossia/detail/small_flat_map.hpp>

namespace score::gfx
{
// Used for the simple case of a single, non-persistent pass (the most common case)

struct SimpleRenderedISFNode : score::gfx::NodeRenderer
{
  SimpleRenderedISFNode(const ISFNode& node) noexcept;

  virtual ~SimpleRenderedISFNode();

  void updateInputTexture(const Port& input, QRhiTexture* tex, QRhiTexture* depthTex = nullptr) override;
  void updateInputSamplerFilter(const Port& input, const RenderTargetSpecs& spec) override;
  QRhiTexture* textureForOutput(const Port& output) override;

  void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void update(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge) override;
  void release(RenderList& r) override;

  void initState(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void releaseState(RenderList& r) override;
  void addOutputPass(RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res) override;
  void removeOutputPass(RenderList& renderer, Edge& edge) override;
  bool hasOutputPassForEdge(Edge& edge) const override;
  void addInputEdge(RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res) override;
  void removeInputEdge(RenderList& renderer, Edge& edge) override;

  void runInitialPasses(
      RenderList&, QRhiCommandBuffer& commands, QRhiResourceUpdateBatch*& res,
      Edge& edge) override;

  void runRenderPass(RenderList&, QRhiCommandBuffer& commands, Edge& edge) override;

private:
  void initPass(
      const TextureRenderTarget& rt, RenderList& renderer, Edge& edge,
      QRhiResourceUpdateBatch& res);
  void initMRTPass(RenderList& renderer, QRhiResourceUpdateBatch& res);
  void initMRTBlitPasses(RenderList& renderer, QRhiResourceUpdateBatch& res);
  void initMRTBlitPass(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge& edge);

  std::vector<Sampler> allSamplers() const noexcept;

  ossia::small_vector<std::pair<Edge*, Pass>, 2> m_passes;

  ISFNode& n;

  std::vector<Sampler> m_inputSamplers;
  std::vector<Sampler> m_audioSamplers;
  ossia::small_flat_map<Edge*, QRhiSampler*, 4> m_blitSamplersByEdge;

  const Mesh* m_mesh{};
  MeshBuffers m_meshBuffer{};

  QRhiBuffer* m_materialUBO{};
  int m_materialSize{};

  std::optional<AudioTextureUpload> m_audioTex;

  // MRT: internally-owned render target with multiple attachments
  TextureRenderTarget m_mrtRenderTarget;
  bool m_hasMRT{false};
  // update() runs once per downstream sink; once-per-frame work is keyed
  // on the RenderList frame counter instead of bools reset in update().
  int64_t m_lastMRTRenderFrame{-1};
  int64_t m_lastStorageSwapFrame{-1};

  // Graphics-visible storage buffers / images (see IsfBindingsBuilder).
  GraphicsStorageResources m_storage;

  // Multiview UBO: N × mat4 view-projection matrices uploaded per frame.
  QRhiBuffer* m_multiViewUBO{};

  // Cached number of bindings consumed by storage resources (recorded in
  // initState so that runtime buffer rebinds can reuse the same layout).
  int m_firstStorageBinding{-1};
};
}
