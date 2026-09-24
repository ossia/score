#pragma once
#include <Gfx/Graph/encoders/GPUVideoEncoder.hpp>

namespace score::gfx
{

/**
 * @brief GPU RGBA -> p216 encoder (semi-planar 4:2:2, 16-bit).
 *
 * The 16-bit sibling of P010Encoder: same two-plane semi-planar shape, but the
 * chroma plane keeps FULL vertical resolution (4:2:2, not 4:2:0) and each
 * sample uses all 16 bits of its word rather than sitting in the high 10.
 * Matches AV_PIX_FMT_P216LE and NDI's NDIlib_FourCC_video_type_P216.
 *
 * Two planes, both 2*width bytes per row over height rows:
 *   - Y : one 16-bit luma per pixel, width x height;
 *   - UV: one interleaved (Cb, Cr) pair per chroma site, width/2 x height.
 * So the two planes are the same size, and a consumer that wants them
 * contiguous (NDI: p_uv = p_y + stride * yres) gets a framestore of
 * 2 * 2*width * height bytes.
 *
 * Feed a >8-bit source texture (RGBA16F) if you want the extra bits to mean
 * anything: from an RGBA8 scene this produces valid P216 whose values are
 * 8-bit quantities scaled into 16-bit words. Consumers cannot tell, and it is
 * still the right thing to send when the wire format is 16-bit, but the
 * precision has to come from the render target.
 *
 * Two implementations selected at compile time, exactly as P010Encoder does and
 * for the same reason (see its comment for the qtbase reference):
 *
 *   - Qt >= 6.10: native R16 (Y) / RG16 (UV) targets, read back tightly.
 *   - Qt <  6.10: the GL backend reads 16-bit single/dual-channel textures back
 *     as 8-bit RGBA, so the 16-bit bytes are packed into RGBA8 here.
 *
 * Both paths expose the same readback byte layout, so consumers are agnostic.
 * Requires width % 2 == 0.
 */
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
struct P216Encoder : GPUVideoEncoder
{
  // %1 = colorMatrixOut() shader defining convert_from_rgb(vec3).
  // R16 is UNORM: writing v in [0,1] stores round(v * 65535), which is the
  // 16-bit sample. No packing helper needed, unlike the 10-bit encoders.
  static constexpr const char* y_frag = R"_(#version 450
    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;
    layout(binding = 3) uniform sampler2D src_tex;
    )_" "%1" R"_(
    vec2 flip_y(vec2 tc) {
    // Only OpenGL. The rest of the engine puts its geometry through
    // renderer.clipSpaceCorrMatrix, which negates Y on Vulkan; this pass draws
    // a hardcoded triangle in raw NDC and does not, so the correction it needs
    // is not the same one. Flipping on Vulkan as well handed libav, GStreamer,
    // NDI and every other consumer an upside-down picture.
    #if defined(QSHADER_SPIRV) || defined(QSHADER_MSL) || defined(QSHADER_HLSL)
      return tc;
    #else
      return vec2(tc.x, 1.0 - tc.y);
    #endif
    }
    void main() {
      vec3 rgb = texture(src_tex, flip_y(v_texcoord)).rgb;
      vec3 yuv = convert_from_rgb(rgb);
      fragColor = vec4(clamp(yuv.x, 0.0, 1.0), 0.0, 0.0, 1.0);
    }
  )_";

  // Rendered at half width, FULL height: 4:2:2 keeps every line's chroma.
  // 4:2:2: average the horizontal pair only. Two texelFetch rather than one
  // bilinear tap: filtering an RGBA8 source quantises to 8 bits on many GPUs.
  static constexpr const char* uv_frag = R"_(#version 450
    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;
    layout(binding = 3) uniform sampler2D src_tex;
    )_" "%1" R"_(
    vec2 flip_y(vec2 tc) {
    #if defined(QSHADER_SPIRV) || defined(QSHADER_MSL) || defined(QSHADER_HLSL)
      return tc;
    #else
      return vec2(tc.x, 1.0 - tc.y);
    #endif
    }
    void main() {
      ivec2 sz = textureSize(src_tex, 0);
      // The source row through flip_y, as in the luma pass: this target has
      // the source's height, so each of its rows is one source row.
      int sy = min(int(floor(flip_y(v_texcoord).y * float(sz.y))), sz.y - 1);
      // Two source pixels per chroma site.
      int x0 = int(floor(v_texcoord.x * float(sz.x / 2))) * 2;
      vec3 yuv0 = convert_from_rgb(
          texelFetch(src_tex, ivec2(min(x0, sz.x - 1), sy), 0).rgb);
      vec3 yuv1 = convert_from_rgb(
          texelFetch(src_tex, ivec2(min(x0 + 1, sz.x - 1), sy), 0).rgb);
      vec2 uv = clamp((yuv0.yz + yuv1.yz) * 0.5, 0.0, 1.0);
      fragColor = vec4(uv, 0.0, 1.0);
    }
  )_";

  QRhiTexture* m_yTexture{};
  QRhiTextureRenderTarget* m_yRT{};
  QRhiRenderPassDescriptor* m_yRP{};
  QRhiShaderResourceBindings* m_ySRB{};
  QRhiGraphicsPipeline* m_yPipeline{};
  QRhiReadbackResult m_yReadback{};

  QRhiTexture* m_uvTexture{};
  QRhiTextureRenderTarget* m_uvRT{};
  QRhiRenderPassDescriptor* m_uvRP{};
  QRhiShaderResourceBindings* m_uvSRB{};
  QRhiGraphicsPipeline* m_uvPipeline{};
  QRhiReadbackResult m_uvReadback{};

  QRhiSampler* m_sampler{};
  int m_width{};
  int m_height{};
  bool m_readbackEnabled{true};

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
        QString::fromLatin1(frag).arg(colorConversion));
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
      int height, const QString& colorConversion) override
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
        rhi, state, inputRGBA, QRhiTexture::RG16, width / 2, height, uv_frag,
        colorConversion, m_uvTexture, m_uvRT, m_uvRP, m_uvSRB, m_uvPipeline);
  }

  void exec(QRhi& rhi, QRhiCommandBuffer& cb) override
  {
    cb.beginPass(m_yRT, Qt::black, {0.0f, 0});
    cb.setGraphicsPipeline(m_yPipeline);
    cb.setShaderResources(m_ySRB);
    cb.setViewport(QRhiViewport(0, 0, m_width, m_height));
    cb.draw(3);
    if(m_readbackEnabled)
    {
      auto* yb = rhi.nextResourceUpdateBatch();
      yb->readBackTexture(QRhiReadbackDescription{m_yTexture}, &m_yReadback);
      cb.endPass(yb);
    }
    else
    {
      cb.endPass();
    }

    cb.beginPass(m_uvRT, Qt::black, {0.0f, 0});
    cb.setGraphicsPipeline(m_uvPipeline);
    cb.setShaderResources(m_uvSRB);
    cb.setViewport(QRhiViewport(0, 0, m_width / 2, m_height));
    cb.draw(3);
    if(m_readbackEnabled)
    {
      auto* uvb = rhi.nextResourceUpdateBatch();
      uvb->readBackTexture(QRhiReadbackDescription{m_uvTexture}, &m_uvReadback);
      cb.endPass(uvb);
    }
    else
    {
      cb.endPass();
    }
  }

  int planeCount() const override { return 2; }

  const QRhiReadbackResult& readback(int plane) const override
  {
    return plane == 0 ? m_yReadback : m_uvReadback;
  }

  /// The plane textures, for consumers that read them back themselves (a
  /// contiguous framestore, a GPU-direct download). Plane 0 is Y, 1 is UV.
  QRhiTexture* planeTexture(int plane) const noexcept
  {
    return plane == 0 ? m_yTexture : m_uvTexture;
  }

  void setReadbackEnabled(bool e) noexcept override { m_readbackEnabled = e; }

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
struct P216Encoder : GPUVideoEncoder
{
  static constexpr const char* common = R"_(
    int flip_y_int(int y, int h) {
    // See GPUVideoEncoder::y_flip_glsl: only OpenGL. The rest of the engine
    // negates Y through renderer.clipSpaceCorrMatrix on Vulkan; this pass
    // indexes texels directly and does not, so it must not flip there.
    #if defined(QSHADER_SPIRV) || defined(QSHADER_MSL) || defined(QSHADER_HLSL)
      return y;
    #else
      return h - 1 - y;
    #endif
    }
    // Component (0=Y,1=U,2=V) as a full-range 16-bit word.
    uint s16(vec3 rgb, int c) {
      vec3 yuv = clamp(convert_from_rgb(rgb), 0.0, 1.0);
      float v = (c == 0) ? yuv.x : (c == 1 ? yuv.y : yuv.z);
      return uint(v * 65535.0 + 0.5);
    }
    vec4 pack2(uint a, uint b) {
      return vec4(float(a & 0xFFu), float((a >> 8u) & 0xFFu),
                  float(b & 0xFFu), float((b >> 8u) & 0xFFu)) / 255.0;
    }
  )_";

  // %1 = colorMatrixOut(), %2 = common. Y: two full-res luma per texel.
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
      uint a = s16(texelFetch(src_tex, ivec2(min(x0,     sz.x - 1), sy), 0).rgb, 0);
      uint b = s16(texelFetch(src_tex, ivec2(min(x0 + 1, sz.x - 1), sy), 0).rgb, 0);
      fragColor = pack2(a, b);
    }
  )_";

  // %1 = colorMatrixOut(), %2 = common. UV: one chroma site per texel, averaged
  // over the horizontal pair on THIS line only -- 4:2:2.
  static constexpr const char* uv_frag = R"_(#version 450
    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;
    layout(binding = 3) uniform sampler2D src_tex;
    )_" "%1" R"_(
    )_" "%2" R"_(
    uint avg2(int x, int y, ivec2 sz, int c) {
      int xa = min(x, sz.x - 1), xb = min(x + 1, sz.x - 1);
      uint s = s16(texelFetch(src_tex, ivec2(xa, y), 0).rgb, c)
             + s16(texelFetch(src_tex, ivec2(xb, y), 0).rgb, c);
      return s >> 1u;
    }
    void main() {
      ivec2 sz = textureSize(src_tex, 0);
      ivec2 o = ivec2(gl_FragCoord.xy);
      int x0 = o.x * 2;
      int sy = flip_y_int(o.y, sz.y);
      fragColor = pack2(avg2(x0, sy, sz, 1), avg2(x0, sy, sz, 2));
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
  bool m_readbackEnabled{true};

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
    if(m_readbackEnabled)
    {
      auto* batch = rhi.nextResourceUpdateBatch();
      batch->readBackTexture(QRhiReadbackDescription{p.texture}, &p.readback);
      cb.endPass(batch);
    }
    else
    {
      cb.endPass();
    }
  }

  void init(
      QRhi& rhi, const RenderState& state, QRhiTexture* inputRGBA, int width,
      int height, const QString& colorConversion) override
  {
    m_width = width;
    m_height = height;
    m_sampler = rhi.newSampler(
        QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    m_sampler->create();

    // Both packed planes are 2*width bytes per row over height rows: the Y
    // texel carries two luma samples, the UV texel one (Cb, Cr) pair.
    const QString y = QString::fromLatin1(y_frag)
                          .arg(colorConversion)
                          .arg(QString::fromLatin1(common));
    const QString uv = QString::fromLatin1(uv_frag)
                           .arg(colorConversion)
                           .arg(QString::fromLatin1(common));
    initPlane(rhi, state, inputRGBA, m_y, width / 2, height, y);
    initPlane(rhi, state, inputRGBA, m_uv, width / 2, height, uv);
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

  QRhiTexture* planeTexture(int plane) const noexcept
  {
    return plane == 0 ? m_y.texture : m_uv.texture;
  }

  void setReadbackEnabled(bool e) noexcept override { m_readbackEnabled = e; }

  void release() override
  {
    m_uv.destroy();
    m_y.destroy();
    delete m_sampler;
    m_sampler = nullptr;
  }
};
#endif

} // namespace score::gfx
