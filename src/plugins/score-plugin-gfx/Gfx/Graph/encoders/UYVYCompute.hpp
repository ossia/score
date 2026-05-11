#pragma once
#include <Gfx/Graph/Utils.hpp>
#include <Gfx/Graph/encoders/ColorSpaceOut.hpp>
#include <Gfx/Graph/encoders/ComputeEncoder.hpp>
#include <Gfx/Graph/encoders/GPUVideoEncoder.hpp>

#include <private/qrhi_p.h>

namespace score::gfx
{

/**
 * @brief Compute-shader RGBA -> UYVY (2vuy) encoder targeting an external SSBO.
 *
 * UYVY packs 2 source pixels into 1 little-endian 32-bit word
 * (Cb, Y0, Cr, Y1) at 8-bit-per-component precision. Each compute thread
 * handles one pixel pair. Designed for the AJA tier-3 path: writes go
 * directly to a CUDA-importable, AJA-DMA-locked GPU buffer.
 */
struct UYVYComputeEncoder final : ComputeEncoder
{
  static constexpr const char* compute_shader = R"_(#version 450
    layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

    layout(binding = 0) uniform sampler2D src_tex;
    layout(std430, binding = 1) writeonly buffer UyvyBuf {
      uint uyvy[];
    };
    layout(std140, binding = 2) uniform Params {
      ivec2 src_size;
      ivec2 _pad0;
      uint  line_stride_words; // bytes-per-row / 4
      uint  pairs_per_row;     // == src_width / 2
      uint  _pad1[2];
    };
    )_" "%1" R"_(

    uint to_byte(float v) {
      return uint(clamp(v, 0.0, 1.0) * 255.0 + 0.5);
    }

    void main() {
      uint pair_x = gl_GlobalInvocationID.x;
      uint y      = gl_GlobalInvocationID.y;
      if (pair_x >= pairs_per_row || int(y) >= src_size.y)
        return;

    #if defined(QSHADER_MSL) || defined(QSHADER_HLSL)
      int src_y = int(y);
    #else
      int src_y = src_size.y - 1 - int(y);
    #endif

      uint x0 = pair_x * 2u;
      vec3 a = clamp(convert_from_rgb(texelFetch(src_tex, ivec2(int(x0     ), src_y), 0).rgb), 0.0, 1.0);
      vec3 b = clamp(convert_from_rgb(texelFetch(src_tex, ivec2(int(x0 + 1u), src_y), 0).rgb), 0.0, 1.0);

      uint Y0 = to_byte(a.x);
      uint Y1 = to_byte(b.x);
      uint Cb = to_byte((a.y + b.y) * 0.5);
      uint Cr = to_byte((a.z + b.z) * 0.5);

      // UYVY little-endian DWORD: Cb, Y0, Cr, Y1
      uint w = Cb | (Y0 << 8) | (Cr << 16) | (Y1 << 24);
      uyvy[y * line_stride_words + pair_x] = w;
    }
  )_";

  QRhiBuffer* m_paramsUBO{};
  QRhiSampler* m_sampler{};
  QRhiShaderResourceBindings* m_srb{};
  QRhiComputePipeline* m_pipeline{};
  int m_width{};
  int m_height{};
  uint32_t m_pairsPerRow{};
  uint32_t m_lineStrideBytes{};

  bool init(
      QRhi& rhi, const RenderState& state, QRhiTexture* inputRGBA, int width,
      int height, QRhiBuffer* outputBuffer,
      const QString& colorConversion = colorMatrixOut()) override
  {
    if(!outputBuffer || (width % 2) != 0)
      return false;
    if(!rhi.isFeatureSupported(QRhi::Compute))
      return false;

    m_width = width;
    m_height = height;
    m_pairsPerRow = width / 2;
    m_lineStrideBytes = width * 2; // UYVY: 2 bytes per pixel, no padding

    m_paramsUBO = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 32);
    m_paramsUBO->setName("UYVYComputeEncoder::params");
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

    QShader cs = makeCompute(
        state, QString::fromLatin1(compute_shader).arg(colorConversion));
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
      uint32_t pairsPerRow;
      uint32_t pad1[2];
    } p{
        m_width, m_height,
        {0, 0},
        m_lineStrideBytes / 4,
        m_pairsPerRow,
        {0, 0}};
    res->updateDynamicBuffer(m_paramsUBO, 0, sizeof(p), &p);

    cb.beginComputePass(res);
    cb.setComputePipeline(m_pipeline);
    cb.setShaderResources(m_srb);
    cb.dispatch((m_pairsPerRow + 31) / 32, m_height, 1);
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
