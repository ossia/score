#pragma once

#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/SharedOutputSettings.hpp>

namespace score::gfx
{

/**
 * @brief Row order PreviewNode writes into its host texture.
 *
 * `true` when the texture's FIRST row -- the row a sampler reaches at v = 0, and
 * row 0 of a QRhi readback -- is the BOTTOM of the picture, which is OpenGL's
 * framebuffer order.
 *
 * Two places have to agree on this and they are not next to each other:
 *
 *  * PreviewNode::createRenderer, which decides whether the graph draws
 *    straight into the host render target or through a Y-flipping blit;
 *  * the host item, which displays the same texture as a 2D quad.
 *    QQuickRhiItem::updatePaintNode picks its UV transform from
 *    QRhi::isYUpInFramebuffer(), i.e. it assumes the texture follows the
 *    BACKEND's order, so wherever this function disagrees with
 *    isYUpInFramebuffer() the item must set `mirrorVertically` to re-flip.
 *
 * The consumer that cannot be fixed up downstream is Qt Quick 3D's
 * `Texture.sourceItem`: QQuickRhiItem is a QSGTextureProvider, so Quick3D takes
 * the provider branch of QQuick3DTexture::updateSpatialNode and hands the raw
 * texture to the material with no per-backend correction at all (and with a
 * CustomMaterial, not even Quick3D's own implicit V flip). It therefore needs
 * ONE row order on every backend. That order is OpenGL's, because that is what
 * every existing document was authored against.
 *
 * Declared here rather than inferred at each call site so that a backend moving
 * between the two renderers changes one statement, not two.
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
