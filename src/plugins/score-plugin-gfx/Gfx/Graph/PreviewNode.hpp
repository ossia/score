#pragma once

#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/SharedOutputSettings.hpp>

namespace score::gfx
{

/**
 * @brief Row order PreviewNode writes into its host texture.
 *
 * `true` when the first row -- what a sampler reaches at v = 0 -- is the bottom
 * of the picture, i.e. OpenGL's framebuffer order. Always OpenGL's, because
 * Quick3D's Texture.sourceItem takes the texture-provider branch and samples
 * the texture raw, so it cannot be corrected downstream and every existing
 * document was authored against that order.
 *
 * createRenderer and the host item (which sets `mirrorVertically` where this
 * disagrees with isYUpInFramebuffer()) must agree, hence one statement here.
 */
SCORE_PLUGIN_GFX_EXPORT bool previewFirstRowIsPictureBottom(QRhi& rhi) noexcept;

class SCORE_PLUGIN_GFX_EXPORT PreviewNode : public score::gfx::OutputNode
{
public:
  explicit PreviewNode(
      Gfx::SharedOutputSettings s, QRhi* rhi, QRhiRenderTarget* tgt, QRhiTexture* tex);
  virtual ~PreviewNode();

  void startRendering() override;
  void onRendererChange() override;
  void render() override;
  bool canRender() const override;
  void stopRendering() override;

  void setRenderer(std::shared_ptr<score::gfx::RenderList> r) override;
  score::gfx::RenderList* renderer() const override;

  void createOutput(score::gfx::OutputConfiguration) override;
  void destroyOutput() override;

  std::shared_ptr<score::gfx::RenderState> renderState() const override;
  score::gfx::OutputNodeRenderer*
  createRenderer(score::gfx::RenderList& r) const noexcept override;
  Configuration configuration() const noexcept override;

  QRhiTexture* texture() const noexcept { return m_texture; }

private:
  Gfx::SharedOutputSettings m_settings;
  QRhi* m_rhi{};

  std::weak_ptr<score::gfx::RenderList> m_renderer{};
  QRhiRenderTarget* m_renderTarget{};
  QRhiTexture* m_texture{};
  std::shared_ptr<score::gfx::RenderState> m_renderState{};
};
}
