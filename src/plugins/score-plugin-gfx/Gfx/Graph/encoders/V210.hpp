#pragma once
#include <Gfx/Graph/encoders/GPUVideoEncoder.hpp>

namespace score::gfx
{

/**
 * @brief Cross-backend RGBA -> v210 encoder.
 *
 * v210 packs 6 source pixels into 4 little-endian 32-bit words (16 bytes):
 *   word 0: Cb01 (10b) | Y0   (10b) | Cr01 (10b) | 2b pad
 *   word 1: Y1   (10b) | Cb23 (10b) | Y2   (10b) | 2b pad
 *   word 2: Cr23 (10b) | Y3   (10b) | Cb45 (10b) | 2b pad
 *   word 3: Y4   (10b) | Cr45 (10b) | Y5   (10b) | 2b pad
 * (4:2:2 chroma subsampling - chroma averaged across adjacent pairs.)
 *
 * Output: RGBA8 texture sized (width/6 * 4) x height. Each output texel is
 * one v210 ULWord, with its 4 bytes laid out as R, G, B, A (= bytes 0..3 of
 * the ULWord on little-endian hosts). The readback bytes are therefore in
 * exact v210 line order and can be DMA'd to the AJA card directly (modulo
 * row-stride padding handled outside this encoder).
 *
 * Source width must be a multiple of 6. For non-multiples use UYVYEncoder
 * + CPU v210 packing instead, or pad the input texture.
 *
 * Single render pass, single readback. Same Y-flip convention as the other
 * encoders (Metal/HLSL no flip, GLSL/SPIRV flip).
 */
struct V210Encoder : GPUVideoEncoder
{
  // %1 = colorMatrixOut() shader defining convert_from_rgb(vec3)
  static constexpr const char* frag = R"_(#version 450
    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;
    layout(binding = 3) uniform sampler2D src_tex;
    )_" "%1" R"_(

    vec2 flip_y_uv(vec2 tc) {
    #if defined(QSHADER_MSL) || defined(QSHADER_HLSL)
      return tc;
    #else
      return vec2(tc.x, 1.0 - tc.y);
    #endif
    }

    int flip_y_int(int y, int h) {
    #if defined(QSHADER_MSL) || defined(QSHADER_HLSL)
      return y;
    #else
      return h - 1 - y;
    #endif
    }

    // 10-bit unsigned (0..1023) for one source pixel.
    uvec3 to_yuv10(vec3 rgb) {
      vec3 yuv = clamp(convert_from_rgb(rgb), 0.0, 1.0);
      return uvec3(yuv * 1023.0 + 0.5);
    }

    // Pack a 32-bit value as 4 little-endian bytes into the RGBA8 output.
    vec4 pack32(uint w) {
      return vec4(
        float( w        & 0xFFu),
        float((w >> 8u) & 0xFFu),
        float((w >> 16u) & 0xFFu),
        float((w >> 24u) & 0xFFu)) / 255.0;
    }

    void main() {
      ivec2 srcSize = textureSize(src_tex, 0);
      ivec2 outPos  = ivec2(gl_FragCoord.xy);

      // Each output row is N v210 words (RGBA8 texels), N == srcWidth/6 * 4.
      int wordInGroup = outPos.x & 3;     // 0..3
      int groupIdx    = outPos.x >> 2;    // 6-pixel group index
      int srcX0       = groupIdx * 6;     // first source pixel in group
      int srcY        = flip_y_int(outPos.y, srcSize.y);

      // texelFetch is clamped via sampler/edge handling; for safety on the
      // last partial group (shouldn't happen since width % 6 == 0) we clamp.
      int x0 = min(srcX0    , srcSize.x - 1);
      int x1 = min(srcX0 + 1, srcSize.x - 1);
      int x2 = min(srcX0 + 2, srcSize.x - 1);
      int x3 = min(srcX0 + 3, srcSize.x - 1);
      int x4 = min(srcX0 + 4, srcSize.x - 1);
      int x5 = min(srcX0 + 5, srcSize.x - 1);

      uint w = 0u;
      if (wordInGroup == 0) {
        uvec3 a = to_yuv10(texelFetch(src_tex, ivec2(x0, srcY), 0).rgb);
        uvec3 b = to_yuv10(texelFetch(src_tex, ivec2(x1, srcY), 0).rgb);
        uint cb01 = (a.y + b.y) >> 1;
        uint cr01 = (a.z + b.z) >> 1;
        w = cb01 | (a.x << 10) | (cr01 << 20);
      } else if (wordInGroup == 1) {
        uvec3 b = to_yuv10(texelFetch(src_tex, ivec2(x1, srcY), 0).rgb);
        uvec3 c = to_yuv10(texelFetch(src_tex, ivec2(x2, srcY), 0).rgb);
        uvec3 d = to_yuv10(texelFetch(src_tex, ivec2(x3, srcY), 0).rgb);
        uint cb23 = (c.y + d.y) >> 1;
        w = b.x | (cb23 << 10) | (c.x << 20);
      } else if (wordInGroup == 2) {
        uvec3 c = to_yuv10(texelFetch(src_tex, ivec2(x2, srcY), 0).rgb);
        uvec3 d = to_yuv10(texelFetch(src_tex, ivec2(x3, srcY), 0).rgb);
        uvec3 e = to_yuv10(texelFetch(src_tex, ivec2(x4, srcY), 0).rgb);
        uvec3 f = to_yuv10(texelFetch(src_tex, ivec2(x5, srcY), 0).rgb);
        uint cr23 = (c.z + d.z) >> 1;
        uint cb45 = (e.y + f.y) >> 1;
        w = cr23 | (d.x << 10) | (cb45 << 20);
      } else {
        uvec3 e = to_yuv10(texelFetch(src_tex, ivec2(x4, srcY), 0).rgb);
        uvec3 f = to_yuv10(texelFetch(src_tex, ivec2(x5, srcY), 0).rgb);
        uint cr45 = (e.z + f.z) >> 1;
        w = e.x | (cr45 << 10) | (f.x << 20);
      }
      fragColor = pack32(w);
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
  int m_outW{};

  void init(
      QRhi& rhi, const RenderState& state, QRhiTexture* inputRGBA, int width,
      int height, const QString& colorConversion = colorMatrixOut()) override
  {
    m_width = width;
    m_height = height;
    // Output: one RGBA8 texel per v210 ULWord. 4 words per 6 source pixels.
    m_outW = (width / 6) * 4;

    m_outTexture = rhi.newTexture(
        QRhiTexture::RGBA8, QSize{m_outW, height}, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    m_outTexture->create();

    m_renderTarget = rhi.newTextureRenderTarget({m_outTexture});
    m_rpDesc = m_renderTarget->newCompatibleRenderPassDescriptor();
    m_renderTarget->setRenderPassDescriptor(m_rpDesc);
    m_renderTarget->create();

    // Nearest sampling - we use texelFetch but a sampler is still required
    // by the binding model.
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

    auto [vertS, fragS] = makeShaders(
        state, QString::fromLatin1(vertex_shader),
        QString::fromLatin1(frag).arg(colorConversion));

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
};

} // namespace score::gfx
