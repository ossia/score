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
 * Requires Qt >= 6.10 for tight R16 GL readback (score builds against 6.11).
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
      PlaneResources& plane, int w, int h, const char* component,
      const QString& colorConversion)
  {
    plane.w = w;
    plane.h = h;
    plane.texture = rhi.newTexture(
        m_bits == 8 ? QRhiTexture::R8 : QRhiTexture::R16, QSize{w, h}, 1,
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

    const QString scale = m_bits == 8 ? QStringLiteral("1.0")
                                      : QStringLiteral("0.0156097");  // 1023/65535
    auto [vs, fs] = makeShaders(
        state, QString::fromLatin1(vertex_shader),
        QString::fromLatin1(plane_frag).arg(colorConversion).arg(component).arg(scale));
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
    initPlane(rhi, state, inputRGBA, m_planes[0], width, height, "x", colorConversion);
    initPlane(rhi, state, inputRGBA, m_planes[1], cw, ch, "y", colorConversion);
    initPlane(rhi, state, inputRGBA, m_planes[2], cw, ch, "z", colorConversion);
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
