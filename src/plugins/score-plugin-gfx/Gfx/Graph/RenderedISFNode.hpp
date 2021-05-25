#pragma once
#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>

#include <ossia/audio/fft.hpp>
#include <ossia/math/math_expression.hpp>

namespace score::gfx
{

struct Pass
{
  TextureRenderTarget renderTarget;
  Pipeline p;
  QRhiBuffer* processUBO{};
};

struct PersistSampler
{
  QRhiSampler* sampler{};
  QRhiTexture* textures[2]{nullptr, nullptr};
};

struct AudioTextureUpload
{
  explicit AudioTextureUpload();

  void process(
      AudioTexture& audio,
      QRhiResourceUpdateBatch& res,
      QRhiTexture* rhiTexture);

  void processTemporal(
      AudioTexture& audio,
      QRhiResourceUpdateBatch& res,
      QRhiTexture* rhiTexture);

  void processSpectral(
      AudioTexture& audio,
      QRhiResourceUpdateBatch& res,
      QRhiTexture* rhiTexture);

  void updateAudioTexture(
      AudioTexture& audio,
      RenderList& renderer,
      QRhiResourceUpdateBatch& res,
      std::vector<Pass>& passes);

private:
  std::vector<float> m_scratchpad;
  ossia::fft m_fft;
};

struct RenderedISFNode : score::gfx::NodeRenderer
{
  RenderedISFNode(const ISFNode& node) noexcept;

  virtual ~RenderedISFNode();
  std::optional<QSize> renderTargetSize() const noexcept override;
  TextureRenderTarget renderTarget() const noexcept override;

  void init(RenderList& renderer) override;
  void update(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void releaseWithoutRenderTarget(RenderList& r) override;
  void release(RenderList& r) override;
  void runPass(
      RenderList& renderer,
      QRhiCommandBuffer& cb,
      QRhiResourceUpdateBatch& res) override;

private:
  std::pair<Pass, Pass>
  createPass(RenderList& renderer, PersistSampler target);
  void initPasses(RenderList& renderer);

  static std::vector<PersistSampler> initPassSamplers(
      ISFNode& n,
      RenderList& renderer,
      int& cur_pos,
      QSize mainTexSize);

  std::vector<Pass> m_passes;
  std::vector<Pass> m_altPasses;

  ISFNode& n;

  std::vector<Sampler> m_inputSamplers;
  std::vector<Sampler> m_audioSamplers;
  std::vector<PersistSampler> m_passSamplers;

  QRhiBuffer* m_meshBuffer{};
  QRhiBuffer* m_idxBuffer{};

  QRhiBuffer* m_materialUBO{};
  int m_materialSize{};
  int64_t materialChangedIndex{-1};

  AudioTextureUpload m_audioTex;
};
}
