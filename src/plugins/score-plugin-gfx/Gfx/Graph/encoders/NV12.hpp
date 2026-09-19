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
 * The UV plane renders into an R8 target of width x height/2, one byte per
 * texel: U in even columns, V in odd ones -- the same byte layout an RG8
 * target at width/2 x height/2 produces.
 *
 * It must not be RG8. An RG8 row here is (w/2) * 2 = w bytes, and where that
 * is not 4-byte aligned the readback comes back the right total SIZE with its
 * rows progressively shifted: row 0 correct, the rest drifting. 722 and 1922
 * are even, so 4:2:0 can express them, and both are real NDI sizes. Measured
 * against a source varying only vertically, where every chroma row must be one
 * repeated value, 287 of 288 rows came back non-constant on OpenGL and on
 * Vulkan; against ffmpeg's own nv12 at 722x576, 30% of bytes differed.
 *
 * Sampling is unchanged -- one bilinear tap at the centre of the 2x2 block --
 * so the values match the RG8 path wherever it was readable at all.
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
      fragColor = vec4(yuv.x, 0.0, 0.0, 1.0);
    }
  )_";

  // Byte b of chroma row cy is U (b even) or V (b odd) of site b/2.
  static constexpr const char* uv_frag = R"_(#version 450
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
      ivec2 sz = textureSize(src_tex, 0);
      // floor() the product, not the product minus half a texel: at output
      // texel i the interpolated value is exactly i + 0.5, so flooring it has
      // half a texel of slack either way where flooring i itself has none.
      int b  = int(floor(v_texcoord.x * float(sz.x)));
      int cy = int(floor(v_texcoord.y * float(sz.y >> 1)));

      vec2 csz = vec2(float(sz.x >> 1), float(sz.y >> 1));
      vec2 tc = (vec2(float(b >> 1), float(cy)) + 0.5) / csz;
      vec3 yuv = convert_from_rgb(texture(src_tex, flip_y(tc)).rgb);
      fragColor = vec4(((b & 1) == 0) ? yuv.y : yuv.z, 0.0, 0.0, 1.0);
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
      int height, const QString& colorConversion) override
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
      // R8, not RG8 -- see the class comment.
      m_uvTexture = rhi.newTexture(
          QRhiTexture::R8, QSize{width, height / 2}, 1,
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

      const char* uv_src = uv_frag;
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

    // Pass 2: UV plane -- R8 at full width, half height, U and V interleaved
    // one byte per texel.
    cb.beginPass(m_uvRT, Qt::black, {0.0f, 0});
    cb.setGraphicsPipeline(m_uvPipeline);
    cb.setShaderResources(m_uvSRB);
    cb.setViewport(QRhiViewport(0, 0, m_width, m_height / 2));
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
