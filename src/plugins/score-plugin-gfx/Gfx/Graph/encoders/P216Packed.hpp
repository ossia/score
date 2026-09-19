#pragma once
#include <Gfx/Graph/encoders/GPUVideoEncoder.hpp>

namespace score::gfx
{

/**
 * @brief GPU RGBA -> p216, as ONE texture holding the whole framestore.
 *
 * P216Encoder produces the same bytes in two plane textures, which is the right
 * shape for a consumer that wants planes. A consumer that wants the framestore
 * -- NDI's p_data, a capture card's frame buffer -- then has to concatenate
 * them, because a QRhi readback lands in its own allocation per plane. At 1080p
 * that is 8.3 MB of memcpy per frame for nothing.
 *
 * P216's two planes are the same size: 2*width bytes per row, height rows each.
 * So the whole framestore is exactly an RGBA8 texture of (width/2) x (2*height)
 * -- luma rows on top, interleaved chroma below -- and one readback of it IS
 * the framestore, in order, ready to send.
 *
 * It is UYVYEncoder's trick (an RGBA8 target at half width whose texels are the
 * wire bytes) applied one dimension further. The cost is that the shader writes
 * bytes rather than samples: each RGBA8 texel carries two 16-bit values, little
 * endian, which is what both P216 and every consumer of it expect.
 *
 * Requires width % 2 == 0. Feed an RGBA16F source texture if the extra bits
 * should mean anything; from an 8-bit scene this produces valid P216 whose
 * values are 8-bit quantities scaled into 16-bit words.
 */
struct P216PackedEncoder : GPUVideoEncoder
{
  // %1 = colorMatrixOut() shader defining convert_from_rgb(vec3)
  static constexpr const char* frag = R"_(#version 450
    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;
    layout(binding = 3) uniform sampler2D src_tex;
    )_" "%1" R"_(

    vec2 flip_y(vec2 tc) {
    // Only OpenGL. The rest of the engine puts its geometry through
    // renderer.clipSpaceCorrMatrix, which negates Y on Vulkan; this pass draws
    // a hardcoded triangle in raw NDC and does not, so the correction it needs
    // is not the same one.
    #if defined(QSHADER_SPIRV) || defined(QSHADER_MSL) || defined(QSHADER_HLSL)
      return tc;
    #else
      return vec2(tc.x, 1.0 - tc.y);
    #endif
    }

    // One 16-bit value as two RGBA8 bytes, little endian.
    vec2 split16(float v) {
      float u = clamp(v, 0.0, 1.0) * 65535.0;
      float hi = floor(u / 256.0);
      float lo = u - hi * 256.0;
      return vec2(lo / 255.0, hi / 255.0);
    }

    void main() {
      vec2 srcSz = vec2(textureSize(src_tex, 0));
      float outRows = srcSz.y * 2.0;

      // Which row of the framestore this fragment is on, and therefore which
      // plane: the luma rows come first, then the chroma rows.
      float outRow = floor(v_texcoord.y * outRows);
      bool chroma = outRow >= srcSz.y;
      float row = chroma ? outRow - srcSz.y : outRow;

      // The source row, with the Y-flip folded in as everywhere else.
      float sy = (flip_y(vec2(0.0, (row + 0.5) / srcSz.y))).y;

      // Two source pixels per output texel, as for UYVY.
      float halfW = srcSz.x * 0.5;
      // floor() the product, not the product minus half a texel: at output
      // texel i the interpolated value is exactly i + 0.5, so flooring it has
      // half a texel of slack either way where flooring i itself has none.
      float outPixel = floor(v_texcoord.x * halfW);
      float sx0 = (outPixel * 2.0 + 0.5) / srcSz.x;
      float sx1 = (outPixel * 2.0 + 1.5) / srcSz.x;

      vec3 yuv0 = convert_from_rgb(texture(src_tex, vec2(sx0, sy)).rgb);
      vec3 yuv1 = convert_from_rgb(texture(src_tex, vec2(sx1, sy)).rgb);

      if(chroma) {
        // One (Cb, Cr) pair per chroma site, averaged across the pixel pair.
        float u = (yuv0.y + yuv1.y) * 0.5;
        float v = (yuv0.z + yuv1.z) * 0.5;
        fragColor = vec4(split16(u), split16(v));
      } else {
        // Two luma samples.
        fragColor = vec4(split16(yuv0.x), split16(yuv1.x));
      }
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
  bool m_readbackEnabled{true};

  void init(
      QRhi& rhi, const RenderState& state, QRhiTexture* inputRGBA, int width,
      int height, const QString& colorConversion) override
  {
    m_width = width;
    m_height = height;

    // The framestore: half width because each texel carries two 16-bit values,
    // twice the height because luma and chroma planes are stacked.
    m_outTexture = rhi.newTexture(
        QRhiTexture::RGBA8, QSize{width / 2, height * 2}, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    m_outTexture->create();

    m_renderTarget = rhi.newTextureRenderTarget({m_outTexture});
    m_rpDesc = m_renderTarget->newCompatibleRenderPassDescriptor();
    m_renderTarget->setRenderPassDescriptor(m_rpDesc);
    m_renderTarget->create();

    // Nearest: every fetch here is at an exact texel centre, and a linear tap
    // would blend neighbouring pixels into the packed bytes.
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
    cb.beginPass(m_renderTarget, Qt::black, {0.0f, 0});
    cb.setGraphicsPipeline(m_pipeline);
    cb.setShaderResources(m_srb);
    cb.setViewport(QRhiViewport(0, 0, m_width / 2, m_height * 2));
    cb.draw(3);

    if(m_readbackEnabled)
    {
      auto* readbackBatch = rhi.nextResourceUpdateBatch();
      QRhiReadbackDescription rb(m_outTexture);
      readbackBatch->readBackTexture(rb, &m_readback);
      cb.endPass(readbackBatch);
    }
    else
    {
      cb.endPass();
    }
  }

  int planeCount() const override { return 1; }

  const QRhiReadbackResult& readback(int) const override { return m_readback; }
  QRhiTexture* outputTexture() const noexcept override { return m_outTexture; }
  void setReadbackEnabled(bool e) noexcept override { m_readbackEnabled = e; }

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
