#pragma once
#include <Gfx/Graph/Utils.hpp>
#include <Gfx/Graph/encoders/ColorSpaceOut.hpp>
#include <Gfx/Graph/encoders/ComputeEncoder.hpp>
#include <Gfx/Graph/encoders/GPUVideoEncoder.hpp>

#include <private/qrhi_p.h>

namespace score::gfx
{

/**
 * @brief Compute-shader RGBA -> v210 encoder targeting an external SSBO.
 *
 * Unlike the fragment V210Encoder, this writes directly to a caller-owned
 * QRhiBuffer (Storage). Designed for the AJA tier-3 path: the buffer is
 * backed by a SHARED, CUDA-importable, AJA-DMA-locked GPU allocation, so
 * the encoder's writes ARE the bytes AJA P2P-DMAs to the card. Zero
 * intermediate copies, zero CUDA-side conversion.
 *
 * Each compute thread handles one 6-pixel v210 group: reads 6 RGBA
 * pixels, applies the colour matrix from %1, averages chroma pairs,
 * packs the four 32-bit v210 words, writes 4 ULWords to the SSBO.
 *
 * Constraints:
 *  - Source width must be a multiple of 6 (1920, 3840, 7680).
 *  - The output buffer's effective layout follows v210 line stride
 *    `((width + 47) / 48) * 128` bytes per row.
 */
struct V210ComputeEncoder final : ComputeEncoder
{
  // %1 = colorMatrixOut(...) shader defining convert_from_rgb(vec3)
  static constexpr const char* compute_shader = R"_(#version 450
    layout(local_size_x = 16, local_size_y = 1, local_size_z = 1) in;

    layout(binding = 0) uniform sampler2D src_tex;
    layout(std430, binding = 1) writeonly buffer V210Buf {
      uint v210[];
    };
    layout(std140, binding = 2) uniform Params {
      ivec2 src_size;
      ivec2 _pad0;
      uint  line_stride_words; // bytes-per-row / 4
      uint  groups_per_row;    // == src_width / 6
      uint  _pad1[2];
    };

    vec2 flip_y(vec2 tc) {
    #if defined(QSHADER_MSL) || defined(QSHADER_HLSL)
      return tc;
    #else
      return vec2(tc.x, 1.0 - tc.y);
    #endif
    }
    )_" "%1" R"_(

    uvec3 to_yuv10(vec3 rgb) {
      vec3 yuv = clamp(convert_from_rgb(rgb), 0.0, 1.0);
      return uvec3(yuv * 1023.0 + 0.5);
    }

    void main() {
      uint group_x = gl_GlobalInvocationID.x;
      uint y       = gl_GlobalInvocationID.y;
      if (group_x >= groups_per_row || int(y) >= src_size.y)
        return;

      // Y-flip on backends that need it (matches the fragment encoders).
      ivec2 srcSize = src_size;
    #if defined(QSHADER_MSL) || defined(QSHADER_HLSL)
      int src_y = int(y);
    #else
      int src_y = srcSize.y - 1 - int(y);
    #endif

      uint x0 = group_x * 6u;
      uvec3 a = to_yuv10(texelFetch(src_tex, ivec2(int(x0    ), src_y), 0).rgb);
      uvec3 b = to_yuv10(texelFetch(src_tex, ivec2(int(x0 + 1u), src_y), 0).rgb);
      uvec3 c = to_yuv10(texelFetch(src_tex, ivec2(int(x0 + 2u), src_y), 0).rgb);
      uvec3 d = to_yuv10(texelFetch(src_tex, ivec2(int(x0 + 3u), src_y), 0).rgb);
      uvec3 e = to_yuv10(texelFetch(src_tex, ivec2(int(x0 + 4u), src_y), 0).rgb);
      uvec3 f = to_yuv10(texelFetch(src_tex, ivec2(int(x0 + 5u), src_y), 0).rgb);

      uint cb01 = (a.y + b.y) >> 1;
      uint cr01 = (a.z + b.z) >> 1;
      uint cb23 = (c.y + d.y) >> 1;
      uint cr23 = (c.z + d.z) >> 1;
      uint cb45 = (e.y + f.y) >> 1;
      uint cr45 = (e.z + f.z) >> 1;

      // v210 packing: 6 pixels = 4 little-endian 32-bit words.
      uint w0 = cb01 | (a.x << 10) | (cr01 << 20);
      uint w1 = b.x  | (cb23 << 10) | (c.x << 20);
      uint w2 = cr23 | (d.x << 10) | (cb45 << 20);
      uint w3 = e.x  | (cr45 << 10) | (f.x << 20);

      uint base = y * line_stride_words + group_x * 4u;
      v210[base + 0u] = w0;
      v210[base + 1u] = w1;
      v210[base + 2u] = w2;
      v210[base + 3u] = w3;
    }
  )_";

  QRhiBuffer* m_paramsUBO{};
  QRhiSampler* m_sampler{};
  QRhiShaderResourceBindings* m_srb{};
  QRhiComputePipeline* m_pipeline{};
  int m_width{};
  int m_height{};
  int m_groupsPerRow{};
  uint32_t m_lineStrideBytes{};

  // Output buffer is owned by the caller. Pass it in init().
  bool init(
      QRhi& rhi, const RenderState& state, QRhiTexture* inputRGBA, int width,
      int height, QRhiBuffer* outputBuffer,
      const QString& colorConversion = colorMatrixOut()) override
  {
    if(!outputBuffer || width % 6 != 0)
      return false;
    if(!rhi.isFeatureSupported(QRhi::Compute))
      return false;

    m_width = width;
    m_height = height;
    m_groupsPerRow = width / 6;
    m_lineStrideBytes = ((width + 47) / 48) * 128;

    // Params UBO (std140, 32 bytes).
    m_paramsUBO = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 32);
    m_paramsUBO->setName("V210ComputeEncoder::params");
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

  // Update params UBO + dispatch the compute pass. Caller has wrapped this
  // inside an offscreen frame; the resource update batch must already be
  // active (typical pattern: pass the batch in, encoder writes the UBO).
  void exec(
      QRhi& rhi, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch* res) override
  {
    struct alignas(16) ParamsData
    {
      int32_t srcW, srcH;
      int32_t pad0[2];
      uint32_t lineStrideWords;
      uint32_t groupsPerRow;
      uint32_t pad1[2];
    } p{
        m_width, m_height,
        {0, 0},
        m_lineStrideBytes / 4,
        static_cast<uint32_t>(m_groupsPerRow),
        {0, 0}};
    res->updateDynamicBuffer(m_paramsUBO, 0, sizeof(p), &p);

    cb.beginComputePass(res);
    cb.setComputePipeline(m_pipeline);
    cb.setShaderResources(m_srb);
    cb.dispatch(
        (m_groupsPerRow + 15) / 16, // local_size_x = 16
        m_height,
        1);
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
