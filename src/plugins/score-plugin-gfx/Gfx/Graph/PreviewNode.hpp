#pragma once

#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/SharedOutputSettings.hpp>

namespace score::gfx
{

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
