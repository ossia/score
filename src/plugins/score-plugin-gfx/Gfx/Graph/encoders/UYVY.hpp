#pragma once
#include <Gfx/Graph/encoders/GPUVideoEncoder.hpp>

namespace score::gfx
{

/**
 * @brief GPU RGBA->UYVY encoder (packed 4:2:2).
 *
 * Output: RGBA8 texture at half width. Each output texel encodes 2 source
 * pixels as (U, Y0, V, Y1). Single render pass, single readback.
 *
 * The readback data is directly in UYVY memory layout, ready for
 * GStreamer `video/x-raw,format=UYVY`.
 */
struct UYVYEncoder : GPUVideoEncoder
{
  // %1 = colorMatrixOut() shader defining convert_from_rgb(vec3)
  static constexpr const char* frag = R"_(#version 450
    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;
    layout(binding = 3) uniform sampler2D src_tex;
    )_" "%1" R"_(

    vec2 flip_y(vec2 tc) {
    #if defined(QSHADER_MSL) || defined(QSHADER_HLSL)
      return tc;
    #else
      return vec2(tc.x, 1.0 - tc.y);
    #endif
    }

    void main() {
      float srcW = float(textureSize(src_tex, 0).x);
      float srcY = flip_y(v_texcoord).y;

      float halfW = srcW * 0.5;
      float outPixel = v_texcoord.x * halfW - 0.5;
      float srcX0 = (floor(outPixel) * 2.0 + 0.5) / srcW;
      float srcX1 = (floor(outPixel) * 2.0 + 1.5) / srcW;

      vec3 rgb0 = texture(src_tex, vec2(srcX0, srcY)).rgb;
      vec3 rgb1 = texture(src_tex, vec2(srcX1, srcY)).rgb;

      vec3 yuv0 = convert_from_rgb(rgb0);
      vec3 yuv1 = convert_from_rgb(rgb1);

      // Average chroma
      float u = (yuv0.y + yuv1.y) * 0.5;
      float v = (yuv0.z + yuv1.z) * 0.5;

      // UYVY: U, Y0, V, Y1
      fragColor = vec4(u, yuv0.x, v, yuv1.x);
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
      int height, const QString& colorConversion = colorMatrixOut()) override
  {
    m_width = width;
    m_height = height;

    // Output: RGBA8 at half width (each texel = 2 source pixels packed as UYVY)
    m_outTexture = rhi.newTexture(
        QRhiTexture::RGBA8, QSize{width / 2, height}, 1,
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

    // Shader resource bindings — only the source texture sampler
    m_srb = rhi.newShaderResourceBindings();
    m_srb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(
            3, QRhiShaderResourceBinding::FragmentStage, inputRGBA, m_sampler),
    });
    m_srb->create();

    // Compile shaders
    auto [vertS, fragS] = makeShaders(
        state, QString::fromLatin1(vertex_shader),
        QString::fromLatin1(frag).arg(colorConversion));

    // Graphics pipeline
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
    cb.setViewport(QRhiViewport(0, 0, m_width / 2, m_height));
    cb.draw(3);

    auto* readbackBatch = rhi.nextResourceUpdateBatch();
    QRhiReadbackDescription rb(m_outTexture);
    readbackBatch->readBackTexture(rb, &m_readback);
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
