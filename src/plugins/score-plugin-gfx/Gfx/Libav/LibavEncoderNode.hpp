#pragma once
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/encoders/GPUVideoEncoder.hpp>
#include <Gfx/InvertYRenderer.hpp>
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
  QRhiRenderBuffer* m_depthStencil{};
  QRhiTextureRenderTarget* m_renderTarget{};
  std::shared_ptr<score::gfx::RenderState> m_renderState{};

  // GPU encoder for YUV conversion (if target is not RGBA)
  std::unique_ptr<score::gfx::GPUVideoEncoder> m_encoder[2];
  int m_encoderIdx{};

  // Double-buffered readback for RGBA path (NDI pattern)
  QRhiReadbackResult m_readback[2];
  QRhiReadbackResult* m_currentReadback{&m_readback[0]};
  Gfx::InvertYRenderer* m_inv_y_renderer{};

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
};
}
