#include <Gfx/Graph/GPUBufferScatter.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <algorithm>
#include <cstdint>

namespace score::gfx
{

static const QString scatterShaderSource = QStringLiteral(R"_(
#version 450

layout(local_size_x = 256) in;

layout(std430, binding = 0) readonly buffer SrcBuf { float src_data[]; };
layout(std430, binding = 1) writeonly buffer DstBuf { float dst_data[]; };
layout(std140, binding = 2) uniform ScatterParams {
    uint element_count;
    uint src_components;
    uint dst_components;
    uint src_stride_floats;
    uint src_offset_floats;
    // Dispatch grid dimensions (in workgroups) along X and Y. When
    // element_count is large enough that the required workgroup count would
    // exceed the backend's per-dimension limit, the host spreads the dispatch
    // across the Y (and Z) axes; the shader must then reconstruct the linear
    // element index from all three gl_GlobalInvocationID components. These are
    // fed via the UBO rather than read from gl_NumWorkGroups because
    // SPIRV-Cross cannot bake gl_NumWorkGroups to HLSL (D3D11/D3D12).
    uint num_workgroups_x;
    uint num_workgroups_y;
    uint _pad2;
};

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

    uint s = src_offset_floats + i * src_stride_floats;
    uint d = i * dst_components;

    // GPU vertex attribute extension convention: (0, 0, 0, 1)
    // Unrolled for the common dst_components values (1-4).

    if(dst_components == 4)
    {
        dst_data[d    ] = (src_components > 0u) ? src_data[s    ] : 0.0;
        dst_data[d + 1] = (src_components > 1u) ? src_data[s + 1] : 0.0;
        dst_data[d + 2] = (src_components > 2u) ? src_data[s + 2] : 0.0;
        dst_data[d + 3] = (src_components > 3u) ? src_data[s + 3] : 1.0;
    }
    else if(dst_components == 3)
    {
        dst_data[d    ] = (src_components > 0u) ? src_data[s    ] : 0.0;
        dst_data[d + 1] = (src_components > 1u) ? src_data[s + 1] : 0.0;
        dst_data[d + 2] = (src_components > 2u) ? src_data[s + 2] : 1.0;
    }
    else if(dst_components == 2)
    {
        dst_data[d    ] = (src_components > 0u) ? src_data[s    ] : 0.0;
        dst_data[d + 1] = (src_components > 1u) ? src_data[s + 1] : 1.0;
    }
    else
    {
        dst_data[d] = (src_components > 0u) ? src_data[s] : 1.0;
    }
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

  // Create the params UBO (std140, 32 bytes)
  op.paramsUBO = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 32);
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
    m_pipeline->create();
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
    uint32_t src_stride_floats;
    uint32_t src_offset_floats;
    uint32_t num_workgroups_x;
    uint32_t num_workgroups_y;
    uint32_t _pad;
  } data{
      p.element_count,
      p.src_components,
      p.dst_components,
      p.src_stride_floats,
      p.src_offset_floats,
      static_cast<uint32_t>(dims.x),
      static_cast<uint32_t>(dims.y),
      0};

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

} // namespace score::gfx
