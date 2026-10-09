#include <Gfx/Graph/GPUIndexWiden.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <algorithm>

namespace score::gfx
{

static const QString widenShaderSource = QStringLiteral(R"_(
#version 450

layout(local_size_x = 256) in;

layout(std430, binding = 0) readonly buffer SrcBuf { uint src_words[]; };
layout(std430, binding = 1) buffer DstBuf { uint dst_indices[]; };
layout(std140, binding = 2) uniform WidenParams {
    uint count;
    uint src_offset_bytes;
    uint dst_offset_words;
    // Workgroups along X, for a dispatch spread across Y past the backend's
    // per-dimension limit.
    uint num_workgroups_x;
};

void main()
{
    uint i = gl_GlobalInvocationID.y * num_workgroups_x * gl_WorkGroupSize.x
             + gl_GlobalInvocationID.x;
    if(i >= count)
        return;
    uint byte_offset = src_offset_bytes + 2u * i;
    uint word = src_words[byte_offset >> 2u];
    uint index = (byte_offset & 2u) != 0u ? (word >> 16u) : (word & 0xFFFFu);
    dst_indices[dst_offset_words + i] = index;
}
)_");

bool GPUIndexWiden::init(RenderState& state)
{
  QRhi& rhi = *state.rhi;
  if(!rhi.isFeatureSupported(QRhi::Compute))
    return false;

  if(const int maxDim = rhi.resourceLimit(QRhi::MaxThreadGroupsPerDimension); maxDim > 0)
    m_maxWorkgroupsPerDim = maxDim;

  try
  {
    m_shader = makeCompute(state, widenShaderSource);
  }
  catch(const std::exception& e)
  {
    qWarning() << "GPUIndexWiden: failed to compile shader:" << e.what();
    return false;
  }

  m_pipeline = rhi.newComputePipeline();
  m_pipeline->setName("GPUIndexWiden");
  m_pipeline->setShaderStage(QRhiShaderStage(QRhiShaderStage::Compute, m_shader));
  return true;
}

void GPUIndexWiden::release()
{
  delete m_pipeline;
  m_pipeline = nullptr;
}

static void setWidenBindings(
    QRhiShaderResourceBindings& srb, const GPUIndexWiden::Params& p, QRhiBuffer* ubo)
{
  srb.setBindings({
      QRhiShaderResourceBinding::bufferLoad(
          0, QRhiShaderResourceBinding::ComputeStage, p.src),
      QRhiShaderResourceBinding::bufferLoadStore(
          1, QRhiShaderResourceBinding::ComputeStage, p.dst),
      QRhiShaderResourceBinding::uniformBuffer(
          2, QRhiShaderResourceBinding::ComputeStage, ubo),
  });
}

GPUIndexWiden::PreparedOp GPUIndexWiden::prepare(QRhi& rhi, const Params& p)
{
  PreparedOp op;
  op.paramsUBO = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 16);
  op.paramsUBO->setName("GPUIndexWiden::params");
  op.paramsUBO->create();

  op.srb = rhi.newShaderResourceBindings();
  op.srb->setName("GPUIndexWiden::srb");
  setWidenBindings(*op.srb, p, op.paramsUBO);
  op.srb->create();

  if(m_pipeline && !m_pipeline->shaderResourceBindings())
  {
    m_pipeline->setShaderResourceBindings(op.srb);
    m_pipeline->create();
  }
  return op;
}

void GPUIndexWiden::releaseOp(PreparedOp& op)
{
  delete op.srb;
  op.srb = nullptr;
  delete op.paramsUBO;
  op.paramsUBO = nullptr;
}

GPUIndexWiden::Dims GPUIndexWiden::dims(uint32_t count) const noexcept
{
  const int64_t maxGroups = std::max(1, m_maxWorkgroupsPerDim);
  const int64_t groups = (int64_t(count) + LocalSize - 1) / LocalSize;
  if(groups <= 0)
    return {};
  if(groups <= maxGroups)
    return {int(groups), 1};
  return {int(maxGroups), int((groups + maxGroups - 1) / maxGroups)};
}

void GPUIndexWiden::updateParams(
    QRhiResourceUpdateBatch& res, const PreparedOp& op, const Params& p)
{
  if(!op.paramsUBO || !op.srb)
    return;
  const uint32_t data[4]{
      p.count, p.src_offset_bytes, p.dst_offset_bytes / 4u, uint32_t(dims(p.count).x)};
  res.updateDynamicBuffer(op.paramsUBO, 0, sizeof(data), data);

  // The buffers may have been reallocated since prepare().
  setWidenBindings(*op.srb, p, op.paramsUBO);
  op.srb->create();
}

void GPUIndexWiden::dispatch(QRhiCommandBuffer& cb, const PreparedOp& op, const Params& p)
{
  if(!m_pipeline || !op.srb)
    return;
  const auto d = dims(p.count);
  if(d.x <= 0)
    return;
  cb.setComputePipeline(m_pipeline);
  cb.setShaderResources(op.srb);
  cb.dispatch(d.x, d.y, 1);
}

}
