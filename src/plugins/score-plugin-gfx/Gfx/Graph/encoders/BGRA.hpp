#pragma once
#include <Gfx/Graph/encoders/GPUVideoEncoder.hpp>

namespace score::gfx
{

/**
 * @brief Cross-backend RGBA -> BGRA byte-order encoder.
 *
 * The shader samples the input RGBA texture and writes (b, g, r, a) to a
 * full-resolution RGBA8 output texture. Because the texture format is RGBA8,
 * the output bytes in memory are in B, G, R, A order — directly matching
 * NTV2_FBF_ARGB or any other API that expects "BGRA byte-order" pixels
 * (DXGI_FORMAT_B8G8R8A8_UNORM, QImage::Format_RGB32, etc.).
 *
 * Single render pass, single readback. Uses the same Y-flip convention as
 * the other encoders so the readback is top-to-bottom on every backend.
 */
struct BGRAEncoder : GPUVideoEncoder
{
  static constexpr const char* frag = R"_(#version 450
    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;
    layout(binding = 3) uniform sampler2D src_tex;

    vec2 flip_y(vec2 tc) {
    #if defined(QSHADER_MSL) || defined(QSHADER_HLSL)
      return tc;
    #else
      return vec2(tc.x, 1.0 - tc.y);
    #endif
    }

    void main() {
      vec4 c = texture(src_tex, flip_y(v_texcoord));
      fragColor = vec4(c.b, c.g, c.r, c.a);
    }
  )_";

  QRhiTexture* m_outTexture{};
  QRhiTextureRenderTarget* m_renderTarget{};
  QRhiRenderPassDescriptor* m_rpDesc{};
  QRhiSampler* m_sampler{};
  QRhiShaderResourceBindings* m_srb{};
  QRhiGraphicsPipeline* m_pipeline{};
  QRhiReadbackResult m_readback{};
  int m_width{};
  int m_height{};

  void init(
      QRhi& rhi, const RenderState& state, QRhiTexture* inputRGBA, int width,
      int height, const QString& /*colorConversion*/ = colorMatrixOut()) override
  {
    m_width = width;
    m_height = height;

    m_outTexture = rhi.newTexture(
        QRhiTexture::RGBA8, QSize{width, height}, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    m_outTexture->create();

    m_renderTarget = rhi.newTextureRenderTarget({m_outTexture});
    m_rpDesc = m_renderTarget->newCompatibleRenderPassDescriptor();
    m_renderTarget->setRenderPassDescriptor(m_rpDesc);
    m_renderTarget->create();

    m_sampler = rhi.newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    m_sampler->create();

    m_srb = rhi.newShaderResourceBindings();
    m_srb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(
            3, QRhiShaderResourceBinding::FragmentStage, inputRGBA, m_sampler),
    });
    m_srb->create();

    auto [vertS, fragS] = makeShaders(
        state, QString::fromLatin1(vertex_shader), QString::fromLatin1(frag));

    m_pipeline = rhi.newGraphicsPipeline();
    m_pipeline->setShaderStages({
        {QRhiShaderStage::Vertex, vertS},
        {QRhiShaderStage::Fragment, fragS},
    });
    m_pipeline->setVertexInputLayout({});
    m_pipeline->setShaderResourceBindings(m_srb);
    m_pipeline->setRenderPassDescriptor(m_rpDesc);
    m_pipeline->create();
  }

  void exec(QRhi& rhi, QRhiCommandBuffer& cb) override
  {
    cb.beginPass(m_renderTarget, Qt::black, {1.0f, 0});
    cb.setGraphicsPipeline(m_pipeline);
    cb.setShaderResources(m_srb);
    cb.setViewport(QRhiViewport(0, 0, m_width, m_height));
    cb.draw(3);

    auto* readbackBatch = rhi.nextResourceUpdateBatch();
    readbackBatch->readBackTexture(QRhiReadbackDescription{m_outTexture}, &m_readback);
    cb.endPass(readbackBatch);
  }

  int planeCount() const override { return 1; }

  const QRhiReadbackResult& readback(int) const override { return m_readback; }

  void release() override
  {
    delete m_pipeline;
    m_pipeline = nullptr;
    delete m_srb;
    m_srb = nullptr;
    delete m_sampler;
    m_sampler = nullptr;
    delete m_rpDesc;
    m_rpDesc = nullptr;
    delete m_renderTarget;
    m_renderTarget = nullptr;
    delete m_outTexture;
    m_outTexture = nullptr;
  }
};

} // namespace score::gfx
