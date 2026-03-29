#pragma once
#include <Gfx/Graph/encoders/GPUVideoEncoder.hpp>

namespace score::gfx
{

/**
 * @brief GPU RGBA->I420 encoder (planar 4:2:0).
 *
 * Three render passes:
 *   Pass 1: Y plane -> R8 at full resolution
 *   Pass 2: U plane -> R8 at half resolution (width/2 × height/2)
 *   Pass 3: V plane -> R8 at half resolution (width/2 × height/2)
 *
 * Three readbacks. The caller concatenates Y + U + V for GStreamer
 * `video/x-raw,format=I420`.
 * 
 * FIXME: is there any way to do it in one pass? MRTs don't seem to work
 * due to the size limitation (same size for everyone)
 */
struct I420Encoder : GPUVideoEncoder
{
  // %1 = colorMatrixOut() shader defining convert_from_rgb(vec3)
  static constexpr const char* y_frag = R"_(#version 450
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
      vec3 yuv = convert_from_rgb(rgb);
      fragColor = vec4(yuv.x, 0.0, 0.0, 1.0);
    }
  )_";

  static constexpr const char* u_frag = R"_(#version 450
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
      vec3 yuv = convert_from_rgb(rgb);
      fragColor = vec4(yuv.y, 0.0, 0.0, 1.0);
    }
  )_";

  static constexpr const char* v_frag = R"_(#version 450
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
      vec3 yuv = convert_from_rgb(rgb);
      fragColor = vec4(yuv.z, 0.0, 0.0, 1.0);
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
      PlaneResources& plane, int w, int h, const char* fragTemplate,
      const QString& colorConversion)
  {
    plane.texture = rhi.newTexture(
        QRhiTexture::R8, QSize{w, h}, 1,
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
        QString::fromLatin1(fragTemplate).arg(colorConversion));
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

  void execPlane(QRhi& rhi, QRhiCommandBuffer& cb, PlaneResources& plane, int w, int h)
  {
    cb.beginPass(plane.rt, Qt::black, {1.0f, 0});
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

    initPlane(rhi, state, inputRGBA, m_planes[0], width, height, y_frag, colorConversion);
    initPlane(rhi, state, inputRGBA, m_planes[1], width / 2, height / 2, u_frag, colorConversion);
    initPlane(rhi, state, inputRGBA, m_planes[2], width / 2, height / 2, v_frag, colorConversion);
  }

  void exec(QRhi& rhi, QRhiCommandBuffer& cb) override
  {
    execPlane(rhi, cb, m_planes[0], m_width, m_height);
    execPlane(rhi, cb, m_planes[1], m_width / 2, m_height / 2);
    execPlane(rhi, cb, m_planes[2], m_width / 2, m_height / 2);
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
