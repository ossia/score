#pragma once
#include <Gfx/Graph/encoders/GPUVideoEncoder.hpp>

namespace score::gfx
{

/**
 * @brief GPU RGBA -> p010le encoder (semi-planar 4:2:0, 10-bit).
 *
 * Each 10-bit sample (0..1023) sits in the HIGH 10 bits of its 16-bit word
 * (low 6 bits zero), matching AV_PIX_FMT_P010LE / GStreamer P010_10LE. Two
 * planes: Y full-res, interleaved UV half-res. Feed a >8-bit source texture
 * (RGBA16F) for real 10-bit precision.
 *
 * Two implementations selected at compile time (see YUV422P10Encoder for the
 * full rationale and the qtbase commit reference):
 *
 *   - Qt >= 6.10: native R16 (Y) / RG16 (UV) targets; QRhi reads them back
 *     tightly. Bilinear sampling averages the 2x2 chroma block.
 *
 *   - Qt < 6.10: the GL backend reads 16-bit single/dual-channel textures back
 *     as 8-bit RGBA, so we pack the bytes into RGBA8 ourselves.
 *
 * BOTH paths expose the SAME readback byte layout (Y = 2w bytes/row over h
 * rows, UV = 2w bytes/row over h/2 rows, U then V interleaved per site), so
 * consumers are agnostic. Requires width % 2 == 0.
 */
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
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
#else  // Qt < 6.10: pack 16-bit samples into RGBA8 for a tight GL readback.
struct P010Encoder : GPUVideoEncoder
{
  static constexpr const char* common = R"_(
    int flip_y_int(int y, int h) {
    #if defined(QSHADER_MSL) || defined(QSHADER_HLSL)
      return y;
    #else
      return h - 1 - y;
    #endif
    }
    // 10-bit component (0=Y,1=U,2=V) shifted into the high 10 bits of 16 bits.
    uint hi10(vec3 rgb, int c) {
      vec3 yuv = clamp(convert_from_rgb(rgb), 0.0, 1.0);
      float v = (c == 0) ? yuv.x : (c == 1 ? yuv.y : yuv.z);
      return uint(v * 1023.0 + 0.5) << 6u;
    }
    vec4 pack2(uint a, uint b) {
      return vec4(float(a & 0xFFu), float((a >> 8u) & 0xFFu),
                  float(b & 0xFFu), float((b >> 8u) & 0xFFu)) / 255.0;
    }
  )_";

  // %1 = colorMatrixOut(), %2 = common. Y: 2 full-res luma per texel.
  static constexpr const char* y_frag = R"_(#version 450
    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;
    layout(binding = 3) uniform sampler2D src_tex;
    )_" "%1" R"_(
    )_" "%2" R"_(
    void main() {
      ivec2 sz = textureSize(src_tex, 0);
      ivec2 o = ivec2(gl_FragCoord.xy);
      int sy = flip_y_int(o.y, sz.y);
      int x0 = o.x * 2;
      uint a = hi10(texelFetch(src_tex, ivec2(min(x0,     sz.x - 1), sy), 0).rgb, 0);
      uint b = hi10(texelFetch(src_tex, ivec2(min(x0 + 1, sz.x - 1), sy), 0).rgb, 0);
      fragColor = pack2(a, b);
    }
  )_";

  // %1 = colorMatrixOut(), %2 = common. UV: one chroma site per texel,
  // averaged over the 2x2 source block; U then V.
  static constexpr const char* uv_frag = R"_(#version 450
    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;
    layout(binding = 3) uniform sampler2D src_tex;
    )_" "%1" R"_(
    )_" "%2" R"_(
    uint avg(int x, int y0, int y1, ivec2 sz, int c) {
      int xa = min(x, sz.x - 1), xb = min(x + 1, sz.x - 1);
      uint s = hi10(texelFetch(src_tex, ivec2(xa, y0), 0).rgb, c)
             + hi10(texelFetch(src_tex, ivec2(xb, y0), 0).rgb, c)
             + hi10(texelFetch(src_tex, ivec2(xa, y1), 0).rgb, c)
             + hi10(texelFetch(src_tex, ivec2(xb, y1), 0).rgb, c);
      return (s >> 2u) & 0xFFC0u; // keep 10 bits in the high position
    }
    void main() {
      ivec2 sz = textureSize(src_tex, 0);
      ivec2 o = ivec2(gl_FragCoord.xy);
      int x0 = o.x * 2;
      int y0 = flip_y_int(o.y * 2,     sz.y);
      int y1 = flip_y_int(o.y * 2 + 1, sz.y);
      fragColor = pack2(avg(x0, y0, y1, sz, 1), avg(x0, y0, y1, sz, 2));
    }
  )_";

  struct PlaneResources
  {
    QRhiTexture* texture{};
    QRhiTextureRenderTarget* rt{};
    QRhiRenderPassDescriptor* rp{};
    QRhiShaderResourceBindings* srb{};
    QRhiGraphicsPipeline* pipeline{};
    QRhiReadbackResult readback{};
    int w{}, h{};

    void destroy()
    {
      delete pipeline; delete srb; delete rp; delete rt; delete texture;
      pipeline = nullptr; srb = nullptr; rp = nullptr; rt = nullptr; texture = nullptr;
    }
  };

  PlaneResources m_y, m_uv;
  QRhiSampler* m_sampler{};
  int m_width{};
  int m_height{};

  void initPlane(
      QRhi& rhi, const RenderState& state, QRhiTexture* inputRGBA,
      PlaneResources& p, int packW, int packH, const QString& frag)
  {
    p.w = packW;
    p.h = packH;
    p.texture = rhi.newTexture(
        QRhiTexture::RGBA8, QSize{packW, packH}, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    p.texture->create();
    p.rt = rhi.newTextureRenderTarget({p.texture});
    p.rp = p.rt->newCompatibleRenderPassDescriptor();
    p.rt->setRenderPassDescriptor(p.rp);
    p.rt->create();
    p.srb = rhi.newShaderResourceBindings();
    p.srb->setBindings({QRhiShaderResourceBinding::sampledTexture(
        3, QRhiShaderResourceBinding::FragmentStage, inputRGBA, m_sampler)});
    p.srb->create();
    auto [vs, fs] = makeShaders(state, QString::fromLatin1(vertex_shader), frag);
    p.pipeline = rhi.newGraphicsPipeline();
    p.pipeline->setShaderStages({
        {QRhiShaderStage::Vertex, vs}, {QRhiShaderStage::Fragment, fs}});
    p.pipeline->setVertexInputLayout({});
    p.pipeline->setShaderResourceBindings(p.srb);
    p.pipeline->setRenderPassDescriptor(p.rp);
    p.pipeline->create();
  }

  void execPlane(QRhi& rhi, QRhiCommandBuffer& cb, PlaneResources& p)
  {
    cb.beginPass(p.rt, Qt::black, {0.0f, 0});
    cb.setGraphicsPipeline(p.pipeline);
    cb.setShaderResources(p.srb);
    cb.setViewport(QRhiViewport(0, 0, p.w, p.h));
    cb.draw(3);
    auto* batch = rhi.nextResourceUpdateBatch();
    batch->readBackTexture(QRhiReadbackDescription{p.texture}, &p.readback);
    cb.endPass(batch);
  }

  void init(
      QRhi& rhi, const RenderState& state, QRhiTexture* inputRGBA, int width,
      int height, const QString& colorConversion = colorMatrixOut()) override
  {
    m_width = width;
    m_height = height;
    m_sampler = rhi.newSampler(
        QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    m_sampler->create();

    const QString cm = QString::fromLatin1(common);
    const QString y = QString::fromLatin1(y_frag).arg(colorConversion, cm);
    const QString uv = QString::fromLatin1(uv_frag).arg(colorConversion, cm);
    initPlane(rhi, state, inputRGBA, m_y, width / 2, height, y);
    initPlane(rhi, state, inputRGBA, m_uv, width / 2, height / 2, uv);
  }

  void exec(QRhi& rhi, QRhiCommandBuffer& cb) override
  {
    execPlane(rhi, cb, m_y);
    execPlane(rhi, cb, m_uv);
  }

  int planeCount() const override { return 2; }

  const QRhiReadbackResult& readback(int plane) const override
  {
    return plane == 0 ? m_y.readback : m_uv.readback;
  }

  void release() override
  {
    m_y.destroy();
    m_uv.destroy();
    delete m_sampler;
    m_sampler = nullptr;
  }
};
#endif

} // namespace score::gfx
