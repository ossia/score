#include <Gfx/Graph/GPUBufferScatter.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <QFloat16>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>

namespace score::gfx
{

static const QString scatterShaderSource = QStringLiteral(R"_(
#version 450

layout(local_size_x = 256) in;

// Raw words on both sides: the source is decoded per src_format, the
// destination written as the bits of a float, uint or int per dst_kind.
layout(std430, binding = 0) readonly buffer SrcBuf { uint src_data[]; };
layout(std430, binding = 1) writeonly buffer DstBuf { uint dst_data[]; };
layout(std140, binding = 2) uniform ScatterParams {
    uint element_count;
    uint src_components;
    uint dst_components;
    uint src_stride_bytes;
    uint src_offset_bytes;
    // Dispatch grid dimensions (in workgroups) along X and Y. When
    // element_count is large enough that the required workgroup count would
    // exceed the backend's per-dimension limit, the host spreads the dispatch
    // across the Y (and Z) axes; the shader must then reconstruct the linear
    // element index from all three gl_GlobalInvocationID components. These are
    // fed via the UBO rather than read from gl_NumWorkGroups because
    // SPIRV-Cross cannot bake gl_NumWorkGroups to HLSL (D3D11/D3D12).
    uint num_workgroups_x;
    uint num_workgroups_y;
    // Stride between destination elements, in words; 0 packs them at
    // dst_components (std430 puts a vec3 every 4 words).
    uint dst_stride_floats;
    // GPUBufferScatter::SourceFormat, GPUBufferScatter::DestinationKind.
    uint src_format;
    uint dst_kind;
};

// The 32 bits at any byte address; past the end reads as zeros.
uint load32(uint b)
{
    uint w = b >> 2u;
    uint n = uint(src_data.length());
    uint lo = w < n ? src_data[w] : 0u;
    uint shift = (b & 3u) * 8u;
    if(shift == 0u)
        return lo;
    uint hi = w + 1u < n ? src_data[w + 1u] : 0u;
    return (lo >> shift) | (hi << (32u - shift));
}

// A float read as an integer, clamped to the integer's range: the plain
// conversion of a negative float to uint, or of one past the range, is
// undefined, and the CPU fallback (scatterOnCpu) clamps the same way.
uint toInteger(float f)
{
    if(dst_kind == 2u)
    {
        if(!(f > -2147483648.0))
            return 0x80000000u;
        if(f >= 2147483648.0)
            return 0x7FFFFFFFu;
        return uint(int(f));
    }
    if(!(f > 0.0))
        return 0u;
    if(f >= 4294967296.0)
        return 0xFFFFFFFFu;
    return uint(f);
}

uint componentBytes()
{
    return src_format == 1u ? 1u : src_format == 2u ? 2u : 4u;
}

// Component c of the source element at byte address b, as the bits of the
// destination kind. A missing component is 0, or 1 when it is the last
// destination component: (0, 0, 0, 1) for four, (0, 0, 1) for three.
uint decodeComponent(uint b, uint c)
{
    bool integer = dst_kind != 0u;
    if(c >= src_components)
        return c == dst_components - 1u ? (integer ? 1u : floatBitsToUint(1.0)) : 0u;
    uint raw = load32(b + c * componentBytes());
    float f;
    uint u;
    if(src_format == 1u)
    {
        // unorm8: the normalized value as a float, the byte as an integer.
        u = raw & 0xFFu;
        f = float(u) / 255.0;
    }
    else if(src_format == 2u)
    {
        f = unpackHalf2x16(raw & 0xFFFFu).x;
        u = toInteger(f);
    }
    else if(src_format == 3u)
    {
        u = raw;
        f = float(raw);
    }
    else if(src_format == 4u)
    {
        u = raw;
        f = float(int(raw));
    }
    else
    {
        f = uintBitsToFloat(raw);
        u = toInteger(f);
    }
    return integer ? u : floatBitsToUint(f);
}

void main()
{
    // Linear index across a possibly multi-axis dispatch. local_size is
    // (256,1,1), so total threads along X = num_workgroups_x * 256, and each Y
    // workgroup contributes one row. Matches the host-side workgroup spread in
    // dispatch() (mirrors RenderedCSFNode's 1D_BUFFER clamp). Over-dispatched
    // threads (i >= element_count) are guarded below, exactly as before.
    uint width_x = num_workgroups_x * gl_WorkGroupSize.x;
    uint i = (gl_GlobalInvocationID.z * num_workgroups_y + gl_GlobalInvocationID.y) * width_x
             + gl_GlobalInvocationID.x;
    if(i >= element_count)
        return;

    uint s = src_offset_bytes + i * src_stride_bytes;
    uint d = i * (dst_stride_floats != 0u ? dst_stride_floats : dst_components);
    for(uint c = 0u; c < dst_components && c < 4u; c++)
        dst_data[d + c] = decodeComponent(s, c);
}
)_");

bool GPUBufferScatter::init(RenderState& state)
{
  QRhi& rhi = *state.rhi;
  if(!rhi.isFeatureSupported(QRhi::Compute))
    return false;

  // Backend's maximum number of workgroups per dispatch dimension (65535 on
  // virtually all Vulkan/GL implementations). Cache it so dispatch()/updateParams
  // can clamp the X axis and spread onto Y/Z, matching RenderedCSFNode.
  const int maxDim = rhi.resourceLimit(QRhi::MaxThreadGroupsPerDimension);
  if(maxDim > 0)
    m_maxWorkgroupsPerDim = maxDim;

  try
  {
    m_shader = makeCompute(state, scatterShaderSource);
  }
  catch(const std::exception& e)
  {
    qWarning() << "GPUBufferScatter: failed to compile shader:" << e.what();
    return false;
  }

  m_pipeline = rhi.newComputePipeline();
  m_pipeline->setShaderStage(QRhiShaderStage(QRhiShaderStage::Compute, m_shader));
  // Pipeline will be finalized on first prepare() when we have an SRB layout.
  return true;
}

void GPUBufferScatter::release()
{
  delete m_pipeline;
  m_pipeline = nullptr;
}

GPUBufferScatter::PreparedOp
GPUBufferScatter::prepare(QRhi& rhi, const Params& p)
{
  PreparedOp op;

  // Create the params UBO (std140: ten uints, rounded up to 48 bytes)
  op.paramsUBO = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 48);
  op.paramsUBO->setName("GPUScatter_params");
  op.paramsUBO->create();

  // Create SRB
  op.srb = rhi.newShaderResourceBindings();
  op.srb->setBindings({
      QRhiShaderResourceBinding::bufferLoad(
          0, QRhiShaderResourceBinding::ComputeStage, p.staging),
      QRhiShaderResourceBinding::bufferStore(
          1, QRhiShaderResourceBinding::ComputeStage, p.output),
      QRhiShaderResourceBinding::uniformBuffer(
          2, QRhiShaderResourceBinding::ComputeStage, op.paramsUBO),
  });
  op.srb->create();

  // Finalize pipeline if not yet done (needs SRB layout)
  if(m_pipeline && !m_pipeline->shaderResourceBindings())
  {
    m_pipeline->setShaderResourceBindings(op.srb);
    // A shader the backend refuses (MSL compilation, say) leaves no pipeline
    // to bind: drop it, dispatch() then does nothing.
    if(!m_pipeline->create())
    {
      qWarning() << "GPUBufferScatter: the scatter pipeline could not be created";
      delete m_pipeline;
      m_pipeline = nullptr;
    }
  }

  return op;
}

GPUBufferScatter::DispatchDims
GPUBufferScatter::computeDispatchDims(uint32_t element_count) const
{
  // Mirror RenderedCSFNode::runInitialPasses' 1D_BUFFER clamp: compute the
  // total workgroup count in int64, then spread across Y (and Z) so no axis
  // exceeds the backend limit. element_count is a uint32, so at LocalSize=256
  // totalWorkgroups <= ceil(2^32 / 256) ≈ 16.7M, always below maxDim^2 — the
  // Z spread is thus unreachable in practice but kept for parity/robustness.
  const int64_t maxWorkgroups
      = m_maxWorkgroupsPerDim > 0 ? m_maxWorkgroupsPerDim : 65535;
  const int64_t totalWorkgroups
      = (static_cast<int64_t>(element_count) + LocalSize - 1) / LocalSize;

  DispatchDims d{0, 0, 0};
  if(totalWorkgroups <= 0)
    return d;

  if(totalWorkgroups > maxWorkgroups * maxWorkgroups)
  {
    d.x = static_cast<int>(maxWorkgroups);
    const int64_t remaining
        = (totalWorkgroups + maxWorkgroups - 1) / maxWorkgroups;
    d.y = static_cast<int>(std::min<int64_t>(remaining, maxWorkgroups));
    d.z = static_cast<int>((remaining + maxWorkgroups - 1) / maxWorkgroups);
  }
  else if(totalWorkgroups > maxWorkgroups)
  {
    d.x = static_cast<int>(std::min<int64_t>(totalWorkgroups, maxWorkgroups));
    d.y = static_cast<int>((totalWorkgroups + maxWorkgroups - 1) / maxWorkgroups);
    d.z = 1;
  }
  else
  {
    d.x = static_cast<int>(totalWorkgroups);
    d.y = 1;
    d.z = 1;
  }
  return d;
}

void GPUBufferScatter::updateParams(
    QRhiResourceUpdateBatch& res, const PreparedOp& op, const Params& p)
{
  if(!op.paramsUBO)
    return;

  // The dispatch grid (below) is recomputed identically in dispatch(); the
  // shader reconstructs its linear index from num_workgroups_x/y, so these
  // MUST match the dims passed to cb.dispatch().
  const DispatchDims dims = computeDispatchDims(p.element_count);

  struct alignas(16) ParamsData
  {
    uint32_t element_count;
    uint32_t src_components;
    uint32_t dst_components;
    uint32_t src_stride_bytes;
    uint32_t src_offset_bytes;
    uint32_t num_workgroups_x;
    uint32_t num_workgroups_y;
    uint32_t dst_stride_floats;
    uint32_t src_format;
    uint32_t dst_kind;
  } data{
      p.element_count,
      p.src_components,
      p.dst_components,
      p.src_stride_bytes,
      p.src_offset_bytes,
      static_cast<uint32_t>(dims.x),
      static_cast<uint32_t>(dims.y),
      p.dst_stride_floats,
      static_cast<uint32_t>(p.src_format),
      static_cast<uint32_t>(p.dst_kind)};

  res.updateDynamicBuffer(op.paramsUBO, 0, sizeof(data), &data);

  // Update SRB bindings (buffers may have changed since prepare)
  op.srb->setBindings({
      QRhiShaderResourceBinding::bufferLoad(
          0, QRhiShaderResourceBinding::ComputeStage, p.staging),
      QRhiShaderResourceBinding::bufferStore(
          1, QRhiShaderResourceBinding::ComputeStage, p.output),
      QRhiShaderResourceBinding::uniformBuffer(
          2, QRhiShaderResourceBinding::ComputeStage, op.paramsUBO),
  });
  op.srb->create();
}

void GPUBufferScatter::dispatch(
    QRhiCommandBuffer& cb, const PreparedOp& op, const Params& p)
{
  if(!m_pipeline || !op.srb)
    return;

  cb.setComputePipeline(m_pipeline);
  cb.setShaderResources(op.srb);

  // Clamp against the backend's per-dimension workgroup limit, spreading onto
  // Y/Z when needed (an unclamped X dispatch of >65535 groups is an invalid
  // dispatch: GL_INVALID_VALUE / VUID-vkCmdDispatch-groupCountX-00386). The
  // shader reconstructs the linear index from num_workgroups_x/y written by
  // updateParams() using this same computation, so they stay consistent.
  const DispatchDims dims = computeDispatchDims(p.element_count);
  if(dims.x <= 0 || dims.y <= 0 || dims.z <= 0)
    return;
  cb.dispatch(dims.x, dims.y, dims.z);
}

void GPUBufferScatter::scatterOnCpu(
    const Params& p, const char* src, int64_t src_size, char* dst, int64_t dst_size)
{
  // Mirrors the shader's decodeComponent(): same decoding, same defaults.
  const bool integer = p.dst_kind != DestinationKind::Float;
  const int compBytes = p.src_format == SourceFormat::Unorm8 ? 1
                        : p.src_format == SourceFormat::Half ? 2
                                                             : 4;
  const auto load = [&](int64_t b) -> uint32_t {
    uint32_t v = 0;
    for(int k = 0; k < 4; k++)
      if(b + k >= 0 && b + k < src_size)
        v |= uint32_t((unsigned char)src[b + k]) << (8 * k);
    return v;
  };
  const auto bits = [](float f) {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    return u;
  };
  const auto decodeComponent = [&](int64_t b, uint32_t c) -> uint32_t {
    if(c >= p.src_components)
      return c == p.dst_components - 1 ? (integer ? 1u : bits(1.f)) : 0u;
    const uint32_t raw = load(b + int64_t(c) * compBytes);
    float f{};
    uint32_t u{};
    // Clamped to the integer's range, as the shader's toInteger: converting a
    // float out of it is undefined behaviour.
    const auto toInt = [&](float v) -> uint32_t {
      if(p.dst_kind == DestinationKind::Int)
      {
        if(!(v > -2147483648.f))
          return uint32_t(std::numeric_limits<int32_t>::min());
        if(v >= 2147483648.f)
          return uint32_t(std::numeric_limits<int32_t>::max());
        return uint32_t(int32_t(v));
      }
      if(!(v > 0.f))
        return 0u;
      if(v >= 4294967296.f)
        return std::numeric_limits<uint32_t>::max();
      return uint32_t(v);
    };
    switch(p.src_format)
    {
      case SourceFormat::Unorm8:
        u = raw & 0xFFu;
        f = float(u) / 255.f;
        break;
      case SourceFormat::Half: {
        const quint16 h = quint16(raw & 0xFFFFu);
        qfloat16 hf;
        std::memcpy(&hf, &h, 2);
        f = float(hf);
        u = toInt(f);
        break;
      }
      case SourceFormat::Uint32:
        u = raw;
        f = float(raw);
        break;
      case SourceFormat::Sint32:
        u = raw;
        f = float(int32_t(raw));
        break;
      case SourceFormat::Float32:
      default:
        std::memcpy(&f, &raw, 4);
        u = toInt(f);
        break;
    }
    return integer ? u : bits(f);
  };

  const uint32_t dstStride = p.dst_stride_floats != 0 ? p.dst_stride_floats : p.dst_components;
  for(uint32_t i = 0; i < p.element_count; i++)
  {
    const int64_t s = int64_t(p.src_offset_bytes) + int64_t(i) * p.src_stride_bytes;
    const int64_t d = int64_t(i) * dstStride * 4;
    for(uint32_t c = 0; c < p.dst_components && c < 4; c++)
    {
      if(d + int64_t(c + 1) * 4 > dst_size)
        return;
      const uint32_t v = decodeComponent(s, c);
      std::memcpy(dst + d + c * 4, &v, 4);
    }
  }
}

} // namespace score::gfx
