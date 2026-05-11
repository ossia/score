#pragma once
#include <Gfx/Graph/Utils.hpp>
#include <Gfx/Graph/encoders/ComputeEncoder.hpp>
#include <Gfx/Graph/encoders/GPUVideoEncoder.hpp>

#include <private/qrhi_p.h>

namespace score::gfx
{

/**
 * @brief Compute-shader RGBA -> BGRA (NTV2_FBF_ARGB byte order) encoder
 *        targeting an external SSBO.
 *
 * One thread per pixel. Reads RGBA, writes one little-endian 32-bit word
 * with bytes (B, G, R, A) in memory order - i.e. AJA's NTV2_FBF_ARGB
 * layout. No colour matrix, no chroma subsampling: this is a passthrough
 * with byte swizzle.
 */
struct BGRAComputeEncoder final : ComputeEncoder
{
  static constexpr const char* compute_shader = R"_(#version 450
    layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

    layout(binding = 0) uniform sampler2D src_tex;
    layout(std430, binding = 1) writeonly buffer BgraBuf {
      uint bgra[];
    };
    layout(std140, binding = 2) uniform Params {
      ivec2 src_size;
      ivec2 _pad0;
      uint  line_stride_words; // bytes-per-row / 4 == width
      uint  _pad1[3];
    };

    uint to_byte(float v) {
      return uint(clamp(v, 0.0, 1.0) * 255.0 + 0.5);
    }

    void main() {
      uint x = gl_GlobalInvocationID.x;
      uint y = gl_GlobalInvocationID.y;
      if (int(x) >= src_size.x || int(y) >= src_size.y)
        return;

    #if defined(QSHADER_MSL) || defined(QSHADER_HLSL)
      int src_y = int(y);
    #else
      int src_y = src_size.y - 1 - int(y);
    #endif

      vec4 c = texelFetch(src_tex, ivec2(int(x), src_y), 0);
      uint B = to_byte(c.b);
      uint G = to_byte(c.g);
      uint R = to_byte(c.r);
      uint A = to_byte(c.a);

      // NTV2_FBF_ARGB byte order in memory: B, G, R, A
      uint w = B | (G << 8) | (R << 16) | (A << 24);
      bgra[y * line_stride_words + x] = w;
    }
  )_";

  QRhiBuffer* m_paramsUBO{};
  QRhiSampler* m_sampler{};
  QRhiShaderResourceBindings* m_srb{};
  QRhiComputePipeline* m_pipeline{};
  int m_width{};
  int m_height{};

  bool init(
      QRhi& rhi, const RenderState& state, QRhiTexture* inputRGBA, int width,
      int height, QRhiBuffer* outputBuffer,
      const QString& /*colorConversion*/ = colorMatrixOut()) override
  {
    if(!outputBuffer || !rhi.isFeatureSupported(QRhi::Compute))
      return false;

    m_width = width;
    m_height = height;

    m_paramsUBO = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 32);
    m_paramsUBO->setName("BGRAComputeEncoder::params");
    if(!m_paramsUBO->create())
      return false;

    m_sampler = rhi.newSampler(
        QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    if(!m_sampler->create())
      return false;

    m_srb = rhi.newShaderResourceBindings();
    m_srb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(
            0, QRhiShaderResourceBinding::ComputeStage, inputRGBA, m_sampler),
        QRhiShaderResourceBinding::bufferStore(
            1, QRhiShaderResourceBinding::ComputeStage, outputBuffer),
        QRhiShaderResourceBinding::uniformBuffer(
            2, QRhiShaderResourceBinding::ComputeStage, m_paramsUBO),
    });
    if(!m_srb->create())
      return false;

    QShader cs = makeCompute(state, QString::fromLatin1(compute_shader));
    m_pipeline = rhi.newComputePipeline();
    m_pipeline->setShaderStage({QRhiShaderStage::Compute, cs});
    m_pipeline->setShaderResourceBindings(m_srb);
    if(!m_pipeline->create())
      return false;

    return true;
  }

  void exec(
      QRhi& rhi, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch* res) override
  {
    struct alignas(16) ParamsData
    {
      int32_t srcW, srcH;
      int32_t pad0[2];
      uint32_t lineStrideWords;
      uint32_t pad1[3];
    } p{m_width, m_height, {0, 0}, static_cast<uint32_t>(m_width), {0, 0, 0}};
    res->updateDynamicBuffer(m_paramsUBO, 0, sizeof(p), &p);

    cb.beginComputePass(res);
    cb.setComputePipeline(m_pipeline);
    cb.setShaderResources(m_srb);
    cb.dispatch((m_width + 15) / 16, (m_height + 15) / 16, 1);
    cb.endComputePass();
  }

  void release() override
  {
    delete m_pipeline;   m_pipeline = nullptr;
    delete m_srb;        m_srb = nullptr;
    delete m_sampler;    m_sampler = nullptr;
    delete m_paramsUBO;  m_paramsUBO = nullptr;
  }
};

} // namespace score::gfx
