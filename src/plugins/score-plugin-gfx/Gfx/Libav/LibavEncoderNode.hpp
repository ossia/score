#pragma once
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Libav/LibavOutputSettings.hpp>

namespace Gfx
{
struct LibavEncoder;
struct LibavEncoderNode : score::gfx::OutputNode
{
  explicit LibavEncoderNode(
      const LibavOutputSettings&, LibavEncoder& encoder, int stream);
  virtual ~LibavEncoderNode();

  LibavEncoder& encoder;
  int stream{};

  std::weak_ptr<score::gfx::RenderList> m_renderer{};
  QRhiTexture* m_texture{};
  QRhiTextureRenderTarget* m_renderTarget{};
  std::shared_ptr<score::gfx::RenderState> m_renderState{};
  bool m_hasSender{};

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

  LibavOutputSettings m_settings;

  QRhiReadbackResult m_readback;
};
}
