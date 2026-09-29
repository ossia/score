#pragma once
#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/IsfBindingsBuilder.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderedISFUtils.hpp>

namespace score::gfx
{
//! Which texture of a persistent pass's pair the pass at `readerPass` samples.
//! The writer renders into textures[frameParity]; the main and alternate pass
//! sets have parity 0 and 1 and swap every frame. As in VVISF, where each pass
//! hands its freshly rendered buffer to the passes drawn after it, a reader
//! after the writer sees this frame and the writer itself and the passes
//! before it see the previous frame.
constexpr int
persistentTextureIndex(int writerPass, int readerPass, int frameParity) noexcept
{
  return writerPass < readerPass ? frameParity : 1 - frameParity;
}

struct RenderedISFNode : score::gfx::NodeRenderer
{
  explicit RenderedISFNode(const ISFNode& node) noexcept;

  virtual ~RenderedISFNode();

  void updateInputTexture(const Port& input, QRhiTexture* tex, QRhiTexture* depthTex = nullptr) override;
  void updateInputSamplerFilter(const Port& input, const RenderTargetSpecs& spec) override;
  void addInputEdge(RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res) override;
  void removeInputEdge(RenderList& renderer, Edge& edge) override;

  void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void update(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* e) override;
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

  QRhiGraphicsPipeline::CompareOp depthCompare() const noexcept override;

private:
  std::pair<Pass, Pass> createPass(
      RenderList& renderer, ossia::small_vector<PassOutput, 1>& m_passSamplers,
      PassOutput target, const isf::pass& modelPass, int passIndex);

  std::pair<Pass, Pass> createFinalPass(
      RenderList& renderer, ossia::small_vector<PassOutput, 1>& m_passSamplers,
      const TextureRenderTarget& target);
  void initPasses(
      const TextureRenderTarget& rt, RenderList& renderer, Edge& edge, QSize mainTexSize,
      QRhiResourceUpdateBatch& res);

  PassOutput initPassSampler(
      ISFNode& n, const isf::pass& pass, int passIndex, RenderList& renderer,
      QSize mainTexSize, QRhiResourceUpdateBatch& res);

  void releaseDetachedPassOutputs() noexcept;

  struct Passes
  {
    ossia::small_vector<Pass, 1> passes;
    ossia::small_vector<Pass, 1> altPasses;
    ossia::small_vector<PassOutput, 1> samplers;
  };

  //! The intermediate pass textures of an edge whose passes were removed. A
  //! persistent pass's history belongs to the node, not to the cable it draws
  //! into: an edge built in the same frame (the output's targets rebuilt, a
  //! node inserted after this one) takes them over. Released on the next
  //! update() otherwise. `output` is ordered so that textures[0] is the one the
  //! next frame writes.
  struct DetachedPassOutput
  {
    int passIndex{};
    PersistSampler output;
  };
  std::vector<DetachedPassOutput> m_detachedPassOutputs;

  std::vector<Sampler> allSamplers(
      ossia::small_vector<PassOutput, 1>&, int readerPass,
      int frameParity) const noexcept;

  ossia::small_vector<std::pair<Edge*, Passes>, 2> m_passes;

  ISFNode& n;

  std::vector<Sampler> m_inputSamplers;
  std::vector<int> m_storageImageSamplers;
  std::vector<Sampler> m_audioSamplers;

  std::vector<TextureRenderTarget> m_innerPassTargets;

  const Mesh* m_mesh{};
  MeshBuffers m_meshBuffer{};

  QRhiBuffer* m_materialUBO{};
  int m_materialSize{};

  AudioTextureUpload m_audioTex;

  // Graphics-visible storage buffers / images declared by the shader
  // (storage_input / csf_image_input / uniform_input). See IsfBindingsBuilder.
  GraphicsStorageResources m_storage;

  // First binding slot reserved for storage resources; determined lazily in
  // initPasses once the pass-sampler count is known (Rendered differs from
  // Simple by having one extra sampler per inner pass).
  int m_firstStorageBinding{-1};
  int m_firstSamplerBinding{3};

  // Guard so the persistent-SSBO state swap runs exactly once per frame even
  // when the node has multiple output edges (each triggers runRenderPass).
  // update() runs once per downstream sink, so once-per-frame work must be
  // keyed on the RenderList's frame counter, not a bool reset in update().
  int64_t m_lastStorageSwapFrame{-1};

  int64_t m_frameIndexFrame{-1};
};

}
