#pragma once
#include <Gfx/Graph/encoders/GPUVideoEncoder.hpp>

namespace score::gfx
{

/**
 * @brief GPU RGBA->NV12 encoder (semi-planar 4:2:0).
 *
 * Two render passes:
 *   Pass 1: Y plane -> R8 texture at full resolution
 *   Pass 2: UV plane -> RG8 texture at half resolution (width/2 × height/2)
 *
 * Two readbacks. The caller concatenates Y + UV data for GStreamer
 * `video/x-raw,format=NV12`.
 */
struct NV12Encoder : GPUVideoEncoder
{
  // %1 = colorMatrixOut() shader defining convert_from_rgb(vec3)
  static constexpr const char* y_frag = R"_(#version 450
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
      vec3 rgb = texture(src_tex, flip_y(v_texcoord)).rgb;
      vec3 yuv = convert_from_rgb(rgb);
      fragColor = vec4(yuv.x, 0.0, 0.0, 1.0);
    }
  )_";

  // Rendered at half resolution. Bilinear sampling averages the 2x2 block.
  static constexpr const char* uv_frag = R"_(#version 450
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
      vec3 rgb = texture(src_tex, flip_y(v_texcoord)).rgb;
      vec3 yuv = convert_from_rgb(rgb);
      fragColor = vec4(yuv.y, yuv.z, 0.0, 1.0);
    }
  )_";

  // Y plane resources
  QRhiTexture* m_yTexture{};
  QRhiTextureRenderTarget* m_yRT{};
  QRhiRenderPassDescriptor* m_yRP{};
  QRhiShaderResourceBindings* m_ySRB{};
  QRhiGraphicsPipeline* m_yPipeline{};
  QRhiReadbackResult m_yReadback{};

  // UV plane resources
  QRhiTexture* m_uvTexture{};
  QRhiTextureRenderTarget* m_uvRT{};
  QRhiRenderPassDescriptor* m_uvRP{};
  QRhiShaderResourceBindings* m_uvSRB{};
  QRhiGraphicsPipeline* m_uvPipeline{};
  QRhiReadbackResult m_uvReadback{};

  // Shared
  QRhiSampler* m_sampler{};
  int m_width{};
  int m_height{};

  void init(
      QRhi& rhi, const RenderState& state, QRhiTexture* inputRGBA, int width,
      int height, const QString& colorConversion = colorMatrixOut()) override
  {
    m_width = width;
    m_height = height;

    m_sampler = rhi.newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    m_sampler->create();

    auto vertSrc = QString::fromLatin1(vertex_shader);

    // Y plane setup
    {
      m_yTexture = rhi.newTexture(
          QRhiTexture::R8, QSize{width, height}, 1,
          QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
      m_yTexture->create();

      m_yRT = rhi.newTextureRenderTarget({m_yTexture});
      m_yRP = m_yRT->newCompatibleRenderPassDescriptor();
      m_yRT->setRenderPassDescriptor(m_yRP);
      m_yRT->create();

      m_ySRB = rhi.newShaderResourceBindings();
      m_ySRB->setBindings({
          QRhiShaderResourceBinding::sampledTexture(
              3, QRhiShaderResourceBinding::FragmentStage, inputRGBA, m_sampler),
      });
      m_ySRB->create();

      auto [vs, fs] = makeShaders(
          state, vertSrc, QString::fromLatin1(y_frag).arg(colorConversion));
      m_yPipeline = rhi.newGraphicsPipeline();
      m_yPipeline->setShaderStages({
          {QRhiShaderStage::Vertex, vs},
          {QRhiShaderStage::Fragment, fs},
      });
      m_yPipeline->setVertexInputLayout({});
      m_yPipeline->setShaderResourceBindings(m_ySRB);
      m_yPipeline->setRenderPassDescriptor(m_yRP);
      m_yPipeline->create();
    }

    // UV plane setup
    {
      m_uvTexture = rhi.newTexture(
          QRhiTexture::RG8, QSize{width / 2, height / 2}, 1,
          QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
      m_uvTexture->create();

      m_uvRT = rhi.newTextureRenderTarget({m_uvTexture});
      m_uvRP = m_uvRT->newCompatibleRenderPassDescriptor();
      m_uvRT->setRenderPassDescriptor(m_uvRP);
      m_uvRT->create();

      m_uvSRB = rhi.newShaderResourceBindings();
      m_uvSRB->setBindings({
          QRhiShaderResourceBinding::sampledTexture(
              3, QRhiShaderResourceBinding::FragmentStage, inputRGBA, m_sampler),
      });
      m_uvSRB->create();

      auto [vs, fs]
          = makeShaders(state, vertSrc, QString::fromLatin1(uv_frag).arg(colorConversion));
      m_uvPipeline = rhi.newGraphicsPipeline();
      m_uvPipeline->setShaderStages({
          {QRhiShaderStage::Vertex, vs},
          {QRhiShaderStage::Fragment, fs},
      });
      m_uvPipeline->setVertexInputLayout({});
      m_uvPipeline->setShaderResourceBindings(m_uvSRB);
      m_uvPipeline->setRenderPassDescriptor(m_uvRP);
      m_uvPipeline->create();
    }
  }

  void exec(QRhi& rhi, QRhiCommandBuffer& cb) override
  {
    // Pass 1: Y plane (full resolution)
    cb.beginPass(m_yRT, Qt::black, {1.0f, 0});
    cb.setGraphicsPipeline(m_yPipeline);
    cb.setShaderResources(m_ySRB);
    cb.setViewport(QRhiViewport(0, 0, m_width, m_height));
    cb.draw(3);

    auto* yReadbackBatch = rhi.nextResourceUpdateBatch();
    yReadbackBatch->readBackTexture(QRhiReadbackDescription{m_yTexture}, &m_yReadback);
    cb.endPass(yReadbackBatch);

    // Pass 2: UV plane (half resolution)
    cb.beginPass(m_uvRT, Qt::black, {1.0f, 0});
    cb.setGraphicsPipeline(m_uvPipeline);
    cb.setShaderResources(m_uvSRB);
    cb.setViewport(QRhiViewport(0, 0, m_width / 2, m_height / 2));
    cb.draw(3);

    auto* uvReadbackBatch = rhi.nextResourceUpdateBatch();
    uvReadbackBatch->readBackTexture(
        QRhiReadbackDescription{m_uvTexture}, &m_uvReadback);
    cb.endPass(uvReadbackBatch);
  }

  int planeCount() const override { return 2; }

  const QRhiReadbackResult& readback(int plane) const override
  {
    return plane == 0 ? m_yReadback : m_uvReadback;
  }

  void release() override
  {
    delete m_uvPipeline;
    delete m_uvSRB;
    delete m_uvRP;
    delete m_uvRT;
    delete m_uvTexture;
    delete m_yPipeline;
    delete m_ySRB;
    delete m_yRP;
    delete m_yRT;
    delete m_yTexture;
    delete m_sampler;
    m_uvPipeline = m_yPipeline = nullptr;
    m_uvSRB = m_ySRB = nullptr;
    m_uvRP = m_yRP = nullptr;
    m_uvRT = m_yRT = nullptr;
    m_uvTexture = m_yTexture = nullptr;
    m_sampler = nullptr;
  }
};

} // namespace score::gfx
