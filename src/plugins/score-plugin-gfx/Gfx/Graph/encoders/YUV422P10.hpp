#pragma once
#include <Gfx/Graph/encoders/GPUVideoEncoder.hpp>

namespace score::gfx
{

/**
 * @brief GPU RGBA -> yuv422p10le encoder (planar 4:2:2, 10-bit).
 *
 * 4:2:2 chroma (U/V are width/2 x height, full vertical resolution); each
 * 10-bit sample (0..1023) sits in the LOW 10 bits of its 16-bit word, matching
 * AV_PIX_FMT_YUV422P10LE / GStreamer I422_10LE. The caller concatenates
 * Y + U + V. Feed a >8-bit source texture (RGBA16F) for real 10-bit precision.
 *
 * Two implementations selected at compile time:
 *
 *   - Qt >= 6.10: native R16 plane targets, one sample per texel. QRhi's GL
 *     backend reads single-/dual-channel 16-bit textures back tightly since
 *     6.10 (qtbase commit e67568706fc8, "rhi: gl: allow readbacks for all
 *     texture formats"); other backends always did. Simpler and cheaper.
 *
 *   - Qt < 6.10: the GL backend reads R16/RG16 back as 8-bit RGBA (the
 *     pre-6.10 readback default-cased everything but R8/float to
 *     glReadPixels(GL_RGBA, GL_UNSIGNED_BYTE)), which destroys 10-bit data. So
 *     we pack the 16-bit plane bytes into RGBA8 ourselves (two samples per
 *     texel, little-endian), exactly like V210Encoder, for a byte-exact
 *     readback on every backend.
 *
 * BOTH paths expose the SAME readback byte layout (Y = 2w bytes/row over h
 * rows, U/V = w bytes/row over h rows), so all consumers are agnostic to which
 * one compiled. The < 6.10 path requires width % 4 == 0; the >= 6.10 path only
 * width % 2 == 0.
 */
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
struct YUV422P10Encoder : GPUVideoEncoder
{
  // %1 = colorMatrixOut() shader defining convert_from_rgb(vec3).
  // %2 = the YUV component selector (x / y / z).
  static constexpr const char* plane_frag = R"_(#version 450
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
      vec3 yuv = clamp(convert_from_rgb(rgb), 0.0, 1.0);
      // 10-bit value packed into the low 10 bits of the R16 word.
      fragColor = vec4(yuv.%2 * (1023.0 / 65535.0), 0.0, 0.0, 1.0);
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

    void destroy()
    {
      delete pipeline;
      delete srb;
      delete rp;
      delete rt;
      delete texture;
      pipeline = nullptr;
      srb = nullptr;
      rp = nullptr;
      rt = nullptr;
      texture = nullptr;
    }
  };

  PlaneResources m_planes[3]; // Y, U, V
  QRhiSampler* m_sampler{};
  int m_width{};
  int m_height{};

  void initPlane(
      QRhi& rhi, const RenderState& state, QRhiTexture* inputRGBA,
      PlaneResources& plane, int w, int h, const char* component,
      const QString& colorConversion)
  {
    plane.texture = rhi.newTexture(
        QRhiTexture::R16, QSize{w, h}, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    plane.texture->create();

    plane.rt = rhi.newTextureRenderTarget({plane.texture});
    plane.rp = plane.rt->newCompatibleRenderPassDescriptor();
    plane.rt->setRenderPassDescriptor(plane.rp);
    plane.rt->create();

    plane.srb = rhi.newShaderResourceBindings();
    plane.srb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(
            3, QRhiShaderResourceBinding::FragmentStage, inputRGBA, m_sampler),
    });
    plane.srb->create();

    auto [vs, fs] = makeShaders(
        state, QString::fromLatin1(vertex_shader),
        QString::fromLatin1(plane_frag).arg(colorConversion).arg(component));
    plane.pipeline = rhi.newGraphicsPipeline();
    plane.pipeline->setShaderStages({
        {QRhiShaderStage::Vertex, vs},
        {QRhiShaderStage::Fragment, fs},
    });
    plane.pipeline->setVertexInputLayout({});
    plane.pipeline->setShaderResourceBindings(plane.srb);
    plane.pipeline->setRenderPassDescriptor(plane.rp);
    plane.pipeline->create();
  }

  void
  execPlane(QRhi& rhi, QRhiCommandBuffer& cb, PlaneResources& plane, int w, int h)
  {
    cb.beginPass(plane.rt, Qt::black, {0.0f, 0});
    cb.setGraphicsPipeline(plane.pipeline);
    cb.setShaderResources(plane.srb);
    cb.setViewport(QRhiViewport(0, 0, w, h));
    cb.draw(3);

    auto* batch = rhi.nextResourceUpdateBatch();
    batch->readBackTexture(QRhiReadbackDescription{plane.texture}, &plane.readback);
    cb.endPass(batch);
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

    // 4:2:2 -> U/V are half-width, full-height.
    initPlane(rhi, state, inputRGBA, m_planes[0], width, height, "x", colorConversion);
    initPlane(rhi, state, inputRGBA, m_planes[1], width / 2, height, "y", colorConversion);
    initPlane(rhi, state, inputRGBA, m_planes[2], width / 2, height, "z", colorConversion);
  }

  void exec(QRhi& rhi, QRhiCommandBuffer& cb) override
  {
    execPlane(rhi, cb, m_planes[0], m_width, m_height);
    execPlane(rhi, cb, m_planes[1], m_width / 2, m_height);
    execPlane(rhi, cb, m_planes[2], m_width / 2, m_height);
  }

  int planeCount() const override { return 3; }

  const QRhiReadbackResult& readback(int plane) const override
  {
    return m_planes[plane].readback;
  }

  void release() override
  {
    for(auto& p : m_planes)
      p.destroy();
    delete m_sampler;
    m_sampler = nullptr;
  }
};
#else  // Qt < 6.10: pack 16-bit samples into RGBA8 for a tight GL readback.
struct YUV422P10Encoder : GPUVideoEncoder
{
  // Helpers shared by all plane shaders (injected as %2).
  static constexpr const char* common = R"_(
    int flip_y_int(int y, int h) {
    #if defined(QSHADER_MSL) || defined(QSHADER_HLSL)
      return y;
    #else
      return h - 1 - y;
    #endif
    }
    uint comp10(vec3 rgb, int c) {
      vec3 yuv = clamp(convert_from_rgb(rgb), 0.0, 1.0);
      float v = (c == 0) ? yuv.x : (c == 1 ? yuv.y : yuv.z);
      return uint(v * 1023.0 + 0.5);
    }
    // Two 16-bit values -> 4 little-endian bytes in an RGBA8 texel.
    vec4 pack2(uint a, uint b) {
      return vec4(float(a & 0xFFu), float((a >> 8u) & 0xFFu),
                  float(b & 0xFFu), float((b >> 8u) & 0xFFu)) / 255.0;
    }
  )_";

  // %1 = colorMatrixOut(), %2 = common. Y plane: 2 full-res luma per texel.
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
      uint a = comp10(texelFetch(src_tex, ivec2(min(x0,     sz.x - 1), sy), 0).rgb, 0);
      uint b = comp10(texelFetch(src_tex, ivec2(min(x0 + 1, sz.x - 1), sy), 0).rgb, 0);
      fragColor = pack2(a, b);
    }
  )_";

  // %1 = colorMatrixOut(), %2 = common, %3 = component (1=U, 2=V).
  // U/V plane: 2 chroma per texel, each = average of 2 adjacent pixels.
  static constexpr const char* uv_frag = R"_(#version 450
    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;
    layout(binding = 3) uniform sampler2D src_tex;
    )_" "%1" R"_(
    )_" "%2" R"_(
    uint chroma(int x, int sy, ivec2 sz) {
      uint a = comp10(texelFetch(src_tex, ivec2(min(x,     sz.x - 1), sy), 0).rgb, %3);
      uint b = comp10(texelFetch(src_tex, ivec2(min(x + 1, sz.x - 1), sy), 0).rgb, %3);
      return (a + b) >> 1u;
    }
    void main() {
      ivec2 sz = textureSize(src_tex, 0);
      ivec2 o = ivec2(gl_FragCoord.xy);
      int sy = flip_y_int(o.y, sz.y);
      int x0 = o.x * 4;
      fragColor = pack2(chroma(x0, sy, sz), chroma(x0 + 2, sy, sz));
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

  PlaneResources m_planes[3]; // Y, U, V (packed RGBA8 targets)
  QRhiSampler* m_sampler{};
  int m_width{};
  int m_height{};

  void initPlane(
      QRhi& rhi, const RenderState& state, QRhiTexture* inputRGBA,
      PlaneResources& plane, int packW, int packH, const QString& frag)
  {
    plane.w = packW;
    plane.h = packH;
    plane.texture = rhi.newTexture(
        QRhiTexture::RGBA8, QSize{packW, packH}, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    plane.texture->create();

    plane.rt = rhi.newTextureRenderTarget({plane.texture});
    plane.rp = plane.rt->newCompatibleRenderPassDescriptor();
    plane.rt->setRenderPassDescriptor(plane.rp);
    plane.rt->create();

    plane.srb = rhi.newShaderResourceBindings();
    plane.srb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(
            3, QRhiShaderResourceBinding::FragmentStage, inputRGBA, m_sampler),
    });
    plane.srb->create();

    auto [vs, fs] = makeShaders(
        state, QString::fromLatin1(vertex_shader), frag);
    plane.pipeline = rhi.newGraphicsPipeline();
    plane.pipeline->setShaderStages({
        {QRhiShaderStage::Vertex, vs}, {QRhiShaderStage::Fragment, fs}});
    plane.pipeline->setVertexInputLayout({});
    plane.pipeline->setShaderResourceBindings(plane.srb);
    plane.pipeline->setRenderPassDescriptor(plane.rp);
    plane.pipeline->create();
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
    const QString u
        = QString::fromLatin1(uv_frag).arg(colorConversion, cm, QStringLiteral("1"));
    const QString v
        = QString::fromLatin1(uv_frag).arg(colorConversion, cm, QStringLiteral("2"));

    // RGBA8 pack widths: Y=w/2 (2 luma/texel), U/V=w/4 (2 chroma/texel).
    initPlane(rhi, state, inputRGBA, m_planes[0], width / 2, height, y);
    initPlane(rhi, state, inputRGBA, m_planes[1], width / 4, height, u);
    initPlane(rhi, state, inputRGBA, m_planes[2], width / 4, height, v);
  }

  void exec(QRhi& rhi, QRhiCommandBuffer& cb) override
  {
    execPlane(rhi, cb, m_planes[0]);
    execPlane(rhi, cb, m_planes[1]);
    execPlane(rhi, cb, m_planes[2]);
  }

  int planeCount() const override { return 3; }

  const QRhiReadbackResult& readback(int plane) const override
  {
    return m_planes[plane].readback;
  }

  void release() override
  {
    for(auto& p : m_planes)
      p.destroy();
    delete m_sampler;
    m_sampler = nullptr;
  }
};
#endif

} // namespace score::gfx
