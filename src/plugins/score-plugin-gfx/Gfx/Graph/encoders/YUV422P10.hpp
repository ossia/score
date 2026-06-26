#pragma once
#include <Gfx/Graph/encoders/GPUVideoEncoder.hpp>

namespace score::gfx
{

/**
 * @brief GPU RGBA -> yuv422p10le encoder (planar 4:2:2, 10-bit).
 *
 * Same structure as I420Encoder, but:
 *   - planes are R16 (16-bit) instead of R8,
 *   - 4:2:2 chroma: U/V are width/2 x height (full vertical resolution),
 *   - each 10-bit sample (0..1023) is written into the LOW 10 bits of the
 *     16-bit word, matching AV_PIX_FMT_YUV422P10LE / GStreamer I422_10LE.
 *
 * Three render passes (Y full-res, U/V half-width), three readbacks. The
 * caller concatenates Y + U + V. Feed the encoder a >8-bit source texture
 * (RGBA16F) for real 10-bit precision.
 *
 * The output is scaled by 1023/65535: an R16 UNORM target stores
 * round(f * 65535), so writing f = v * (1023/65535) stores round(v * 1023) —
 * i.e. the 10-bit value in the low 10 bits, high 6 bits zero.
 */
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

} // namespace score::gfx
