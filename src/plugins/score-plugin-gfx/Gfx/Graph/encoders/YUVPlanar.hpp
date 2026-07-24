#pragma once
#include <Gfx/Graph/encoders/GPUVideoEncoder.hpp>

#include <memory>

namespace score::gfx
{

/**
 * @brief Generic 3-plane planar YCbCr encoder (8 or 10 bit, 4:2:2 or 4:2:0).
 *
 * Covers the AJA planar framestore formats beyond the 10-bit 4:2:2 case that
 * YUV422P10Encoder already handles:
 *   - NTV2_FBF_8BIT_YCBCR_422PL3      (bits=8,  chromaShiftY=0)
 *   - NTV2_FBF_8BIT_YCBCR_420PL3      (bits=8,  chromaShiftY=1)
 *   - NTV2_FBF_10BIT_YCBCR_420PL3_LE  (bits=10, chromaShiftY=1)
 *
 * Each plane is rendered at its own resolution into an R8 (8-bit) or R16
 * (10-bit, sample in the low 10 bits, AV_PIX_FMT_*P10LE convention) target;
 * chroma is downsampled by linear filtering of the source. The multi-plane
 * readback path in AJAOutputNode copies each plane using the NTV2 format
 * descriptor's per-plane stride, so the readback layout (Y full-res, Cb/Cr at
 * w/2 and h>>chromaShiftY) maps straight onto the AJA framestore.
 *
 * The 10-bit mode has two implementations selected at compile time (see
 * YUV422P10Encoder for the full rationale and the qtbase commit reference):
 *
 *   - Qt >= 6.10: native R16 plane targets, one sample per texel; QRhi reads
 *     them back tightly on every backend.
 *
 *   - Qt < 6.10: the GL backend reads R16 targets back as 8-bit RGBA, which
 *     destroys 10-bit data. We pack the 16-bit little-endian plane bytes into
 *     RGBA8 targets ourselves (two samples per texel), exactly like the
 *     YUV422P10Encoder / P010Encoder fallbacks. This path requires
 *     width % 4 == 0 (chroma pack width is w/4 texels).
 *
 * BOTH paths expose the SAME readback byte layout (Y = 2w bytes/row over h
 * rows, Cb/Cr = w bytes/row over h>>chromaShiftY rows), so consumers are
 * agnostic. The 8-bit mode uses R8 targets, which read back tightly on all
 * Qt versions (the pre-6.10 GL readback special-cases R8).
 */
struct YUVPlanarEncoder : GPUVideoEncoder
{
  // %1 = colorMatrixOut() defining convert_from_rgb(vec3)
  // %2 = component selector (x/y/z)
  // %3 = output scale (1.0 for R8, 1023.0/65535.0 for R16)
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
      fragColor = vec4(yuv.%2 * float(%3), 0.0, 0.0, 1.0);
    }
  )_";

#if QT_VERSION < QT_VERSION_CHECK(6, 10, 0)
  // Qt < 6.10 10-bit fallback: pack 16-bit LE samples into RGBA8 targets.
  // Helpers shared by the packed plane shaders (injected as %2).
  static constexpr const char* pack_common = R"_(
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

  // %1 = colorMatrixOut(), %2 = pack_common. Y plane: 2 full-res luma/texel.
  static constexpr const char* pack_y_frag = R"_(#version 450
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

  // %1 = colorMatrixOut(), %2 = pack_common, %3 = component (1=Cb, 2=Cr),
  // %4 = chromaShiftY (0: 4:2:2, 1: 4:2:0). Each chroma sample averages the
  // horizontal pair, and additionally the vertical pair when VS == 1 (for
  // VS == 0 the y0/y1 rows coincide, so the 4-tap sum / 4 is the 2-tap mean).
  static constexpr const char* pack_c_frag = R"_(#version 450
    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;
    layout(binding = 3) uniform sampler2D src_tex;
    )_" "%1" R"_(
    )_" "%2" R"_(
    uint chroma(int x, int oy, ivec2 sz) {
      const int VS = %4;
      int xa = min(x,     sz.x - 1);
      int xb = min(x + 1, sz.x - 1);
      int y0 = flip_y_int(min((oy << VS),      sz.y - 1), sz.y);
      int y1 = flip_y_int(min((oy << VS) + VS, sz.y - 1), sz.y);
      uint s = comp10(texelFetch(src_tex, ivec2(xa, y0), 0).rgb, %3)
             + comp10(texelFetch(src_tex, ivec2(xb, y0), 0).rgb, %3)
             + comp10(texelFetch(src_tex, ivec2(xa, y1), 0).rgb, %3)
             + comp10(texelFetch(src_tex, ivec2(xb, y1), 0).rgb, %3);
      return s >> 2u;
    }
    void main() {
      ivec2 sz = textureSize(src_tex, 0);
      ivec2 o = ivec2(gl_FragCoord.xy);
      int x0 = o.x * 4;
      fragColor = pack2(chroma(x0, o.y, sz), chroma(x0 + 2, o.y, sz));
    }
  )_";
#endif

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
      pipeline = nullptr; srb = nullptr; rp = nullptr; rt = nullptr;
      texture = nullptr;
    }
  };

  int m_bits{8};
  int m_chromaShiftY{0};
  PlaneResources m_planes[3]; // Y, Cb, Cr
  QRhiSampler* m_sampler{};
  int m_width{};
  int m_height{};

  YUVPlanarEncoder(int bits, int chromaShiftY)
      : m_bits{bits}
      , m_chromaShiftY{chromaShiftY}
  {
  }

  void initPlane(
      QRhi& rhi, const RenderState& state, QRhiTexture* inputRGBA,
      PlaneResources& plane, QRhiTexture::Format fmt, int w, int h,
      const QString& frag)
  {
    plane.w = w;
    plane.h = h;
    plane.texture = rhi.newTexture(
        fmt, QSize{w, h}, 1,
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

    auto [vs, fs] = makeShaders(state, QString::fromLatin1(vertex_shader), frag);
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
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    m_sampler->create();

    const int cw = width / 2;
    const int ch = height >> m_chromaShiftY;

#if QT_VERSION < QT_VERSION_CHECK(6, 10, 0)
    if(m_bits > 8)
    {
      // Qt < 6.10's GL backend reads R16 targets back as 8-bit RGBA; pack the
      // 16-bit little-endian bytes into RGBA8 ourselves (two samples/texel)
      // so the readback layout matches the native R16 path. Needs w % 4 == 0.
      const QString cm = QString::fromLatin1(pack_common);
      const QString vshift = QString::number(m_chromaShiftY);
      const QString y = QString::fromLatin1(pack_y_frag).arg(colorConversion, cm);
      const QString cb = QString::fromLatin1(pack_c_frag)
                             .arg(colorConversion, cm, QStringLiteral("1"), vshift);
      const QString cr = QString::fromLatin1(pack_c_frag)
                             .arg(colorConversion, cm, QStringLiteral("2"), vshift);
      initPlane(
          rhi, state, inputRGBA, m_planes[0], QRhiTexture::RGBA8, width / 2, height,
          y);
      initPlane(
          rhi, state, inputRGBA, m_planes[1], QRhiTexture::RGBA8, width / 4, ch, cb);
      initPlane(
          rhi, state, inputRGBA, m_planes[2], QRhiTexture::RGBA8, width / 4, ch, cr);
      return;
    }
#endif

    const QString scale = m_bits == 8 ? QStringLiteral("1.0")
                                      : QStringLiteral("0.0156097"); // 1023/65535
    auto frag = [&](const char* component) {
      return QString::fromLatin1(plane_frag)
          .arg(colorConversion)
          .arg(component)
          .arg(scale);
    };
    const auto fmt = m_bits == 8 ? QRhiTexture::R8 : QRhiTexture::R16;
    initPlane(rhi, state, inputRGBA, m_planes[0], fmt, width, height, frag("x"));
    initPlane(rhi, state, inputRGBA, m_planes[1], fmt, cw, ch, frag("y"));
    initPlane(rhi, state, inputRGBA, m_planes[2], fmt, cw, ch, frag("z"));
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

  // NTV2_FBF_8BIT_YCBCR_422PL3
  static std::unique_ptr<YUVPlanarEncoder> p422_8()
  {
    return std::make_unique<YUVPlanarEncoder>(8, 0);
  }
  // NTV2_FBF_8BIT_YCBCR_420PL3
  static std::unique_ptr<YUVPlanarEncoder> p420_8()
  {
    return std::make_unique<YUVPlanarEncoder>(8, 1);
  }
  // NTV2_FBF_10BIT_YCBCR_420PL3_LE
  static std::unique_ptr<YUVPlanarEncoder> p420_10()
  {
    return std::make_unique<YUVPlanarEncoder>(10, 1);
  }
};

} // namespace score::gfx
