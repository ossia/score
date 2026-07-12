#pragma once
#include <Gfx/Graph/encoders/GPUVideoEncoder.hpp>

namespace score::gfx
{

/**
 * @brief GPU RGBA->NV12 encoder (semi-planar 4:2:0).
 *
 * Two render passes:
 *   Pass 1: Y plane -> R8 texture at full resolution
 *   Pass 2: UV plane -> interleaved chroma at half vertical resolution
 *
 * Two readbacks. The caller concatenates Y + UV data for GStreamer
 * `video/x-raw,format=NV12`.
 *
 * Two UV implementations selected at compile time (see YUV422P10Encoder for
 * the full rationale and the qtbase commit reference):
 *
 *   - Qt >= 6.10: RG8 target at width/2 × height/2; QRhi reads it back
 *     tightly (2 bytes per chroma site). Bilinear sampling averages the
 *     2x2 source block.
 *
 *   - Qt < 6.10: the GL backend reads RG8 render targets back RGBA-expanded
 *     (4 bytes per site, U,V,0,255) — the pre-6.10 readback special-cases
 *     only R8 and float formats. So we render the interleaved U,V bytes into
 *     an R8 target of width × height/2 instead: one byte per texel, U in
 *     even columns, V in odd ones, each averaged over the same 2x2 block.
 *
 * BOTH paths expose the SAME readback byte layout (w bytes per row over h/2
 * rows, U then V per site), so consumers are agnostic. The Y plane is R8 and
 * reads back tightly everywhere.
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

#if QT_VERSION < QT_VERSION_CHECK(6, 10, 0)
  // Qt < 6.10 fallback: interleaved UV bytes in an R8 target (width ×
  // height/2). Output texel x holds U (x even) or V (x odd) of chroma site
  // x/2, averaged over the 2x2 source block like the bilinear RG8 path.
  static constexpr const char* uv_frag_packed = R"_(#version 450
    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;
    layout(binding = 3) uniform sampler2D src_tex;
    )_" "%1" R"_(
    int flip_y_int(int y, int h) {
    #if defined(QSHADER_MSL) || defined(QSHADER_HLSL)
      return y;
    #else
      return h - 1 - y;
    #endif
    }
    float chroma(ivec2 p, int c) {
      vec3 yuv = clamp(convert_from_rgb(texelFetch(src_tex, p, 0).rgb), 0.0, 1.0);
      return c == 1 ? yuv.y : yuv.z;
    }
    void main() {
      ivec2 sz = textureSize(src_tex, 0);
      ivec2 o = ivec2(gl_FragCoord.xy);
      int c = ((o.x & 1) == 0) ? 1 : 2;   // even byte: U, odd byte: V
      int x0 = (o.x >> 1) * 2;
      int xa = min(x0,     sz.x - 1);
      int xb = min(x0 + 1, sz.x - 1);
      int y0 = flip_y_int(min(o.y * 2,     sz.y - 1), sz.y);
      int y1 = flip_y_int(min(o.y * 2 + 1, sz.y - 1), sz.y);
      float s = chroma(ivec2(xa, y0), c) + chroma(ivec2(xb, y0), c)
              + chroma(ivec2(xa, y1), c) + chroma(ivec2(xb, y1), c);
      fragColor = vec4(s * 0.25, 0.0, 0.0, 1.0);
    }
  )_";
#endif

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
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
      m_uvTexture = rhi.newTexture(
          QRhiTexture::RG8, QSize{width / 2, height / 2}, 1,
          QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
#else
      // Tight RG8 GL readback needs Qt >= 6.10; use an R8 target holding the
      // interleaved U,V bytes directly (same readback byte layout).
      m_uvTexture = rhi.newTexture(
          QRhiTexture::R8, QSize{width, height / 2}, 1,
          QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
#endif
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

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
      const char* uv_src = uv_frag;
#else
      const char* uv_src = uv_frag_packed;
#endif
      auto [vs, fs]
          = makeShaders(state, vertSrc, QString::fromLatin1(uv_src).arg(colorConversion));
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
    cb.beginPass(m_yRT, Qt::black, {0.0f, 0});
    cb.setGraphicsPipeline(m_yPipeline);
    cb.setShaderResources(m_ySRB);
    cb.setViewport(QRhiViewport(0, 0, m_width, m_height));
    cb.draw(3);

    auto* yReadbackBatch = rhi.nextResourceUpdateBatch();
    yReadbackBatch->readBackTexture(QRhiReadbackDescription{m_yTexture}, &m_yReadback);
    cb.endPass(yReadbackBatch);

    // Pass 2: UV plane (half resolution; on Qt < 6.10 the target is R8 at
    // full width with U,V interleaved per texel)
    cb.beginPass(m_uvRT, Qt::black, {0.0f, 0});
    cb.setGraphicsPipeline(m_uvPipeline);
    cb.setShaderResources(m_uvSRB);
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    cb.setViewport(QRhiViewport(0, 0, m_width / 2, m_height / 2));
#else
    cb.setViewport(QRhiViewport(0, 0, m_width, m_height / 2));
#endif
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
