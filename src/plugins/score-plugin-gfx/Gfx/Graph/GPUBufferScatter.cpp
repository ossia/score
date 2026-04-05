#include <Gfx/Graph/GPUBufferScatter.hpp>
#include <Gfx/Graph/Utils.hpp>

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
    uint _pad0, _pad1, _pad2;
};

void main()
{
    uint i = gl_GlobalInvocationID.x;
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

void GPUBufferScatter::updateParams(
    QRhiResourceUpdateBatch& res, const PreparedOp& op, const Params& p)
{
  if(!op.paramsUBO)
    return;

  struct alignas(16) ParamsData
  {
    uint32_t element_count;
    uint32_t src_components;
    uint32_t dst_components;
    uint32_t src_stride_floats;
    uint32_t src_offset_floats;
    uint32_t _pad[3];
  } data{
      p.element_count,
      p.src_components,
      p.dst_components,
      p.src_stride_floats,
      p.src_offset_floats,
      {0, 0, 0}};

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

  const int workgroups = (p.element_count + LocalSize - 1) / LocalSize;
  cb.dispatch(workgroups, 1, 1);
}

} // namespace score::gfx
