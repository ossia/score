#pragma once
#include <Gfx/Graph/encoders/GPUVideoEncoder.hpp>

namespace score::gfx
{

/**
 * @brief GPU RGBA -> p010le encoder (semi-planar 4:2:0, 10-bit).
 *
 * Same structure as NV12Encoder, but:
 *   - planes are R16 / RG16 (16-bit) instead of R8 / RG8,
 *   - each 10-bit sample (0..1023) is written into the HIGH 10 bits of the
 *     16-bit word with the low 6 bits zero, matching AV_PIX_FMT_P010LE /
 *     GStreamer P010_10LE.
 *
 * Two render passes (Y full-res, interleaved UV half-res), two readbacks.
 * Feed a >8-bit source texture (RGBA16F) for real 10-bit precision.
 *
 * Packing: an R16 UNORM target stores round(f * 65535). Writing
 * f = round(v*1023) * 64 / 65535 stores round(v*1023) << 6 — the 10-bit value
 * in the high 10 bits, low 6 bits zero, which is exactly P010's layout.
 */
struct P010Encoder : GPUVideoEncoder
{
  static constexpr const char* pack_fn = R"_(
    // 10-bit value -> high 10 bits of a 16-bit UNORM word (low 6 bits zero).
    float pack10hi(float v) {
      return round(clamp(v, 0.0, 1.0) * 1023.0) * 64.0 / 65535.0;
    }
  )_";

  // %1 = colorMatrixOut() shader defining convert_from_rgb(vec3)
  static constexpr const char* y_frag = R"_(#version 450
    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;
    layout(binding = 3) uniform sampler2D src_tex;
    )_" "%1" R"_(
    )_" "%2" R"_(
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
      fragColor = vec4(pack10hi(yuv.x), 0.0, 0.0, 1.0);
    }
  )_";

  // Rendered at half resolution. Bilinear sampling averages the 2x2 block.
  static constexpr const char* uv_frag = R"_(#version 450
    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;
    layout(binding = 3) uniform sampler2D src_tex;
    )_" "%1" R"_(
    )_" "%2" R"_(
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
      fragColor = vec4(pack10hi(yuv.y), pack10hi(yuv.z), 0.0, 1.0);
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

  QRhiSampler* m_sampler{};
  int m_width{};
  int m_height{};

  void setupPlane(
      QRhi& rhi, const RenderState& state, QRhiTexture* inputRGBA,
      QRhiTexture::Format fmt, int w, int h, const char* frag,
      const QString& colorConversion, QRhiTexture*& tex,
      QRhiTextureRenderTarget*& rt, QRhiRenderPassDescriptor*& rp,
      QRhiShaderResourceBindings*& srb, QRhiGraphicsPipeline*& pipeline)
  {
    tex = rhi.newTexture(
        fmt, QSize{w, h}, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    tex->create();

    rt = rhi.newTextureRenderTarget({tex});
    rp = rt->newCompatibleRenderPassDescriptor();
    rt->setRenderPassDescriptor(rp);
    rt->create();

    srb = rhi.newShaderResourceBindings();
    srb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(
            3, QRhiShaderResourceBinding::FragmentStage, inputRGBA, m_sampler),
    });
    srb->create();

    auto [vs, fs] = makeShaders(
        state, QString::fromLatin1(vertex_shader),
        QString::fromLatin1(frag).arg(colorConversion).arg(pack_fn));
    pipeline = rhi.newGraphicsPipeline();
    pipeline->setShaderStages({
        {QRhiShaderStage::Vertex, vs},
        {QRhiShaderStage::Fragment, fs},
    });
    pipeline->setVertexInputLayout({});
    pipeline->setShaderResourceBindings(srb);
    pipeline->setRenderPassDescriptor(rp);
    pipeline->create();
  }

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

    setupPlane(
        rhi, state, inputRGBA, QRhiTexture::R16, width, height, y_frag,
        colorConversion, m_yTexture, m_yRT, m_yRP, m_ySRB, m_yPipeline);
    setupPlane(
        rhi, state, inputRGBA, QRhiTexture::RG16, width / 2, height / 2, uv_frag,
        colorConversion, m_uvTexture, m_uvRT, m_uvRP, m_uvSRB, m_uvPipeline);
  }

  void exec(QRhi& rhi, QRhiCommandBuffer& cb) override
  {
    cb.beginPass(m_yRT, Qt::black, {0.0f, 0});
    cb.setGraphicsPipeline(m_yPipeline);
    cb.setShaderResources(m_ySRB);
    cb.setViewport(QRhiViewport(0, 0, m_width, m_height));
    cb.draw(3);
    auto* yb = rhi.nextResourceUpdateBatch();
    yb->readBackTexture(QRhiReadbackDescription{m_yTexture}, &m_yReadback);
    cb.endPass(yb);

    cb.beginPass(m_uvRT, Qt::black, {0.0f, 0});
    cb.setGraphicsPipeline(m_uvPipeline);
    cb.setShaderResources(m_uvSRB);
    cb.setViewport(QRhiViewport(0, 0, m_width / 2, m_height / 2));
    cb.draw(3);
    auto* uvb = rhi.nextResourceUpdateBatch();
    uvb->readBackTexture(QRhiReadbackDescription{m_uvTexture}, &m_uvReadback);
    cb.endPass(uvb);
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
