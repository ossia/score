#pragma once
#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderedISFUtils.hpp>

namespace score::gfx
{
struct RenderedISFNode : score::gfx::NodeRenderer
{
  RenderedISFNode(const ISFNode& node) noexcept;

  virtual ~RenderedISFNode();

  TextureRenderTarget renderTargetForInput(const Port& p) override;

  void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void update(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* e) override;
  void release(RenderList& r) override;

  void runInitialPasses(
      RenderList&, QRhiCommandBuffer& commands, QRhiResourceUpdateBatch*& res,
      Edge& edge) override;

  void runRenderPass(RenderList&, QRhiCommandBuffer& commands, Edge& edge) override;

private:
  ossia::small_flat_map<const Port*, TextureRenderTarget, 2> m_rts;

  std::pair<Pass, Pass> createPass(
      RenderList& renderer, ossia::small_vector<PassOutput, 1>& m_passSamplers,
      PassOutput target, bool previousPassIsPersistent);

  std::pair<Pass, Pass> createFinalPass(
      RenderList& renderer, ossia::small_vector<PassOutput, 1>& m_passSamplers,
      const TextureRenderTarget& target);
  void initPasses(
      const TextureRenderTarget& rt, RenderList& renderer, Edge& edge, int& cur_pos,
      QSize mainTexSize, QRhiResourceUpdateBatch& res);

  PassOutput initPassSampler(
      ISFNode& n, const isf::pass& pass, RenderList& renderer, int& cur_pos,
      QSize mainTexSize, QRhiResourceUpdateBatch& res);

  struct Passes
  {
    ossia::small_vector<Pass, 1> passes;
    ossia::small_vector<Pass, 1> altPasses;
    ossia::small_vector<PassOutput, 1> samplers;
  };

  std::vector<Sampler>
  allSamplers(ossia::small_vector<PassOutput, 1>&, int mainOrAltPass) const noexcept;

  ossia::small_vector<std::pair<Edge*, Passes>, 2> m_passes;

  ISFNode& n;

  std::vector<Sampler> m_inputSamplers;
  std::vector<Sampler> m_audioSamplers;

  std::vector<TextureRenderTarget> m_innerPassTargets;

  const Mesh* m_mesh{};
  QRhiBuffer* m_meshBuffer{};
  QRhiBuffer* m_idxBuffer{};

  QRhiBuffer* m_materialUBO{};
  int m_materialSize{};

  AudioTextureUpload m_audioTex;
};

// Used for the simple case of a single, non-persistent pass (the most common case)

struct SimpleRenderedISFNode : score::gfx::NodeRenderer
{
  SimpleRenderedISFNode(const ISFNode& node) noexcept;

  virtual ~SimpleRenderedISFNode();

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

  void initPass(const TextureRenderTarget& rt, RenderList& renderer, Edge& edge);

  std::vector<Sampler> allSamplers() const noexcept;

  ossia::small_vector<std::pair<Edge*, Pass>, 2> m_passes;

  ISFNode& n;

  std::vector<Sampler> m_inputSamplers;
  std::vector<Sampler> m_audioSamplers;

  const Mesh* m_mesh{};
  QRhiBuffer* m_meshBuffer{};
  QRhiBuffer* m_idxBuffer{};

  QRhiBuffer* m_materialUBO{};
  int m_materialSize{};

  std::optional<AudioTextureUpload> m_audioTex;
};

struct SimpleRenderedVSANode : score::gfx::NodeRenderer
{
  SimpleRenderedVSANode(const ISFNode& node) noexcept;

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
