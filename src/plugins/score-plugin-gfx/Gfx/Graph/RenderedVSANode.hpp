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

  void updateInputTexture(const Port& input, QRhiTexture* tex, QRhiTexture* depthTex = nullptr) override;
  void updateInputSamplerFilter(const Port& input, const RenderTargetSpecs& spec) override;

  void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void update(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge) override;
  void release(RenderList& r) override;

  void initState(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void releaseState(RenderList& renderer) override;
  void addOutputPass(
      RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res) override;
  void removeOutputPass(RenderList& renderer, Edge& edge) override;
  bool hasOutputPassForEdge(Edge& edge) const override;

  void runInitialPasses(
      RenderList&, QRhiCommandBuffer& commands, QRhiResourceUpdateBatch*& res,
      Edge& edge) override;

  void runRenderPass(RenderList&, QRhiCommandBuffer& commands, Edge& edge) override;

private:
  void initPass(
      const TextureRenderTarget& rt, RenderList& renderer, Edge& edge,
      QRhiResourceUpdateBatch& res);

  std::vector<Sampler> allSamplers() const noexcept;

  void initTouchHistory(RenderList& renderer, QRhiResourceUpdateBatch& res);
  void updateTouchHistory(RenderList& renderer, QRhiResourceUpdateBatch& res);
  void uploadTouchHistory(RenderList& renderer, QRhiResourceUpdateBatch& res);
  void bindTouchSource(RenderList& renderer);

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

  // Primitive modes QRhi cannot draw directly are drawn from an index list
  // over the vertex ids, rebuilt when the count changes: LINE_LOOP as a line
  // strip 0, 1, ..., N - 1, 0, and TRI_FAN on a backend without triangle fans
  // as triangles {0, i - 1, i}.
  enum class IndexList : uint8_t
  {
    None,
    LineLoop,
    TriangleFan
  } m_indexList{};
  QRhiBuffer* m_indices{};
  int m_indexVertexCount{-1};

  QRhiBuffer* m_materialUBO{};
  int m_materialSize{};

  std::optional<AudioTextureUpload> m_audioTex;

  int m_prevFormat{};
  int64_t m_frameIndexFrame{-1};

  // vertexshaderart's `touch` history (greggman/vertexshaderart, main.js):
  // 32 columns, one per pointer, 240 rows of (x, y, pressure, time), the
  // newest in row 0. Bound to the `touch` image input while nothing is cabled
  // into it. m_touchRows is newest first.
  static constexpr int touchColumns = 32;
  static constexpr int touchRows = 240;
  const Port* m_touchPort{};
  int m_touchPortIndex{-1};
  int m_touchSampler{-1};
  QRhiTexture* m_touchTex{};
  bool m_touchHalf{};
  bool m_touchBound{};
  std::vector<float> m_touchHistory;
  int64_t m_touchFrame{-1};
};
}
