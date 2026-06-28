#pragma once
#include <Gfx/Graph/encoders/GPUVideoEncoder.hpp>

namespace score::gfx
{

/**
 * @brief Generic packed-pixel RGB/byte encoder for AJA framebuffer formats.
 *
 * Generalises the V210 "RGBA8 output texel == 4 bytes of the destination row"
 * technique to any byte-packed layout. The destination row is treated as a
 * flat byte array; the fragment shader emits 4 consecutive row bytes per
 * output texel via a per-format `rowByte(b, srcY)` function. The readback is
 * therefore already in exact AJA line order and only needs a row-stride memcpy.
 *
 * A format is described by:
 *   - bytesPerGroup / pixelsPerGroup : the repeating unit (e.g. 9 bytes / 2 px
 *     for 12-bit packed, 4 bytes / 1 px for 10-bit RGB).
 *   - maxVal  : per-component full-scale (255 / 1023 / 4095).
 *   - alphaVal: constant alpha emitted for formats that carry it (10-bit ARGB).
 *   - rowByteBody : GLSL body of `uint rowByte(int b, int srcY)`. It may call
 *     `uvec4 rgb_at(int x, int srcY)` which returns the source pixel's
 *     components quantised to [0..maxVal] (with .a == alphaVal).
 *
 * The output RGBA8 texture width is (width * bytesPerGroup)/(pixelsPerGroup*4);
 * all AJA RGB line pitches are 4-byte aligned, and HD/UHD widths are multiples
 * of 8, so this is always integral for the formats wired here.
 *
 * These are RGB framebuffer formats: the card's CSC does RGB->SDI, so the
 * encoder writes RGB directly (no RGB->YCbCr matrix). >8-bit precision comes
 * from the RGBA16F intermediate the output node renders into.
 */
struct PackedRGBEncoder : GPUVideoEncoder
{
  static constexpr const char* frag_template = R"_(#version 450
    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;
    layout(binding = 3) uniform sampler2D src_tex;

    vec2 flip_y_uv(vec2 tc) {
    #if defined(QSHADER_MSL) || defined(QSHADER_HLSL)
      return tc;
    #else
      return vec2(tc.x, 1.0 - tc.y);
    #endif
    }

    const uint MAXV  = %1u;
    const uint ALPHA = %2u;
    ivec2 g_srcSize;

    // Source pixel components quantised to [0..MAXV], .a == ALPHA.
    uvec4 rgb_at(int x, int srcY) {
      x = clamp(x, 0, g_srcSize.x - 1);
      vec3 c = clamp(texelFetch(src_tex, ivec2(x, srcY), 0).rgb, 0.0, 1.0);
      uvec3 v = uvec3(c * float(MAXV) + 0.5);
      return uvec4(v, ALPHA);
    }

    uint rowByte(int b, int srcY) {
%3
    }

    void main() {
      g_srcSize = textureSize(src_tex, 0);
      int srcY = clamp(
          int(flip_y_uv(v_texcoord).y * float(g_srcSize.y)), 0, g_srcSize.y - 1);
      int o = int(gl_FragCoord.x) * 4;
      fragColor = vec4(
          float(rowByte(o,     srcY)),
          float(rowByte(o + 1, srcY)),
          float(rowByte(o + 2, srcY)),
          float(rowByte(o + 3, srcY))) / 255.0;
    }
  )_";

  int m_bytesPerGroup{4};
  int m_pixelsPerGroup{1};
  int m_maxVal{1023};
  int m_alphaVal{0};
  QString m_rowByteBody;

  PackedRGBEncoder(
      int bytesPerGroup, int pixelsPerGroup, int maxVal, int alphaVal,
      QString rowByteBody)
      : m_bytesPerGroup{bytesPerGroup}
      , m_pixelsPerGroup{pixelsPerGroup}
      , m_maxVal{maxVal}
      , m_alphaVal{alphaVal}
      , m_rowByteBody{std::move(rowByteBody)}
  {
  }

  QRhiTexture* m_outTexture{};
  QRhiTextureRenderTarget* m_renderTarget{};
  QRhiRenderPassDescriptor* m_rpDesc{};
  QRhiSampler* m_sampler{};
  QRhiShaderResourceBindings* m_srb{};
  QRhiGraphicsPipeline* m_pipeline{};
  QRhiReadbackResult m_readback{};
  int m_width{};
  int m_height{};
  int m_outW{};

  void init(
      QRhi& rhi, const RenderState& state, QRhiTexture* inputRGBA, int width,
      int height, const QString& /*colorConversion*/ = colorMatrixOut()) override
  {
    m_width = width;
    m_height = height;
    m_outW = (width * m_bytesPerGroup) / (m_pixelsPerGroup * 4);

    m_outTexture = rhi.newTexture(
        QRhiTexture::RGBA8, QSize{m_outW, height}, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    m_outTexture->create();

    m_renderTarget = rhi.newTextureRenderTarget({m_outTexture});
    m_rpDesc = m_renderTarget->newCompatibleRenderPassDescriptor();
    m_renderTarget->setRenderPassDescriptor(m_rpDesc);
    m_renderTarget->create();

    m_sampler = rhi.newSampler(
        QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    m_sampler->create();

    m_srb = rhi.newShaderResourceBindings();
    m_srb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(
            3, QRhiShaderResourceBinding::FragmentStage, inputRGBA, m_sampler),
    });
    m_srb->create();

    const QString fragSrc = QString::fromLatin1(frag_template)
                                .arg(m_maxVal)
                                .arg(m_alphaVal)
                                .arg(m_rowByteBody);
    auto [vertS, fragS] = makeShaders(
        state, QString::fromLatin1(vertex_shader), fragSrc);

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
    cb.beginPass(m_renderTarget, Qt::black, {0.0f, 0});
    cb.setGraphicsPipeline(m_pipeline);
    cb.setShaderResources(m_srb);
    cb.setViewport(QRhiViewport(0, 0, m_outW, m_height));
    cb.draw(3);

    auto* readbackBatch = rhi.nextResourceUpdateBatch();
    readbackBatch->readBackTexture(QRhiReadbackDescription{m_outTexture}, &m_readback);
    cb.endPass(readbackBatch);
  }

  int planeCount() const override { return 1; }
  const QRhiReadbackResult& readback(int) const override { return m_readback; }

  void release() override
  {
    delete m_pipeline;     m_pipeline = nullptr;
    delete m_srb;          m_srb = nullptr;
    delete m_sampler;      m_sampler = nullptr;
    delete m_rpDesc;       m_rpDesc = nullptr;
    delete m_renderTarget; m_renderTarget = nullptr;
    delete m_outTexture;   m_outTexture = nullptr;
  }

  // ---- Per-format factories (bit layouts verified against the SDK's
  //      PackRGB10Bit* / Convert16BitARGBTo* / ConvertLine_8bitABGR_* ) ----

  // NTV2_FBF_10BIT_RGB: word = (B<<20)|(G<<10)|R, 4 bytes LE, 1 px/word.
  static std::unique_ptr<PackedRGBEncoder> rgb10()
  {
    return std::make_unique<PackedRGBEncoder>(4, 1, 1023, 0, R"_(
      int px = b >> 2; int k = b & 3;
      uvec4 c = rgb_at(px, srcY);
      uint w = c.r | (c.g << 10) | (c.b << 20);
      return (w >> uint(8 * k)) & 0xFFu;
    )_");
  }

  // NTV2_FBF_10BIT_DPX (big-endian): value=(R<<22)|(G<<12)|(B<<2), stored BE.
  static std::unique_ptr<PackedRGBEncoder> dpx10be()
  {
    return std::make_unique<PackedRGBEncoder>(4, 1, 1023, 0, R"_(
      int px = b >> 2; int k = b & 3;
      uvec4 c = rgb_at(px, srcY);
      uint w = (c.r << 22) | (c.g << 12) | (c.b << 2);
      return (w >> uint(8 * (3 - k))) & 0xFFu;
    )_");
  }

  // NTV2_FBF_10BIT_DPX_LE: same value, little-endian.
  static std::unique_ptr<PackedRGBEncoder> dpx10le()
  {
    return std::make_unique<PackedRGBEncoder>(4, 1, 1023, 0, R"_(
      int px = b >> 2; int k = b & 3;
      uvec4 c = rgb_at(px, srcY);
      uint w = (c.r << 22) | (c.g << 12) | (c.b << 2);
      return (w >> uint(8 * k)) & 0xFFu;
    )_");
  }

  // NTV2_FBF_24BIT_RGB: bytes [R,G,B], 3 B/px.
  static std::unique_ptr<PackedRGBEncoder> rgb24()
  {
    return std::make_unique<PackedRGBEncoder>(3, 1, 255, 0, R"_(
      int px = b / 3; int k = b - px * 3;
      uvec4 c = rgb_at(px, srcY);
      if (k == 0) return c.r;
      if (k == 1) return c.g;
      return c.b;
    )_");
  }

  // NTV2_FBF_24BIT_BGR: bytes [B,G,R], 3 B/px.
  static std::unique_ptr<PackedRGBEncoder> bgr24()
  {
    return std::make_unique<PackedRGBEncoder>(3, 1, 255, 0, R"_(
      int px = b / 3; int k = b - px * 3;
      uvec4 c = rgb_at(px, srcY);
      if (k == 0) return c.b;
      if (k == 1) return c.g;
      return c.r;
    )_");
  }

  // NTV2_FBF_48BIT_RGB: R16,G16,B16 little-endian, full 16-bit range, 6 B/px.
  // (The card reads this as full 16-bit, not 12-bit-in-low-bits — verified by
  // round-trip: 12-bit values read back at ~6% brightness.)
  static std::unique_ptr<PackedRGBEncoder> rgb48()
  {
    return std::make_unique<PackedRGBEncoder>(6, 1, 65535, 0, R"_(
      int px = b / 6; int k = b - px * 6;
      uvec4 c = rgb_at(px, srcY);
      int comp = k >> 1; int hi = k & 1;
      uint v = comp == 0 ? c.r : (comp == 1 ? c.g : c.b);
      return hi == 1 ? ((v >> 8) & 0xFFu) : (v & 0xFFu);
    )_");
  }

  // NTV2_FBF_12BIT_RGB_PACKED: 12-bit data, left-justified in 16 (v=val<<4),
  // 2 px packed into 9 bytes. Layout per Convert16BitARGBTo12BitRGBPacked.
  static std::unique_ptr<PackedRGBEncoder> rgb12packed()
  {
    return std::make_unique<PackedRGBEncoder>(9, 2, 4095, 0, R"_(
      int grp = b / 9; int k = b - grp * 9; int px0 = grp * 2;
      uvec4 c0 = rgb_at(px0, srcY);
      uvec4 c1 = rgb_at(px0 + 1, srcY);
      uint VR0 = c0.r << 4, VG0 = c0.g << 4, VB0 = c0.b << 4;
      uint VR1 = c1.r << 4, VG1 = c1.g << 4, VB1 = c1.b << 4;
      if (k == 0) return (VR0 >> 8) & 0xFFu;
      if (k == 1) return (VR0 & 0xF0u) | ((VG0 >> 12) & 0x0Fu);
      if (k == 2) return (VG0 >> 4) & 0xFFu;
      if (k == 3) return (VB0 >> 8) & 0xFFu;
      if (k == 4) return (VB0 & 0xF0u) | ((VR1 >> 12) & 0x0Fu);
      if (k == 5) return (VR1 >> 4) & 0xFFu;
      if (k == 6) return (VG1 >> 8) & 0xFFu;
      if (k == 7) return (VG1 & 0xF0u) | ((VB1 >> 12) & 0x0Fu);
      return (VB1 >> 4) & 0xFFu;
    )_");
  }

  // NTV2_FBF_10BIT_ARGB: B,G,R,A 10-bit packed into 5 bytes/px.
  static std::unique_ptr<PackedRGBEncoder> argb10()
  {
    return std::make_unique<PackedRGBEncoder>(5, 1, 1023, 1023, R"_(
      int px = b / 5; int k = b - px * 5;
      uvec4 c = rgb_at(px, srcY);
      uint R = c.r, G = c.g, B = c.b, A = c.a;
      if (k == 0) return B & 0xFFu;
      if (k == 1) return ((B >> 8) & 0x03u) | ((G & 0x3Fu) << 2);
      if (k == 2) return ((G >> 6) & 0x0Fu) | ((R & 0x0Fu) << 4);
      if (k == 3) return ((R >> 4) & 0x3Fu) | ((A & 0x03u) << 6);
      return (A >> 2) & 0xFFu;
    )_");
  }
};

} // namespace score::gfx
