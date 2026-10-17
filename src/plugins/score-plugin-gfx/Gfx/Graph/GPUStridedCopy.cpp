#include <Gfx/Graph/GPUStridedCopy.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <algorithm>

namespace score::gfx
{

static const QString stridedCopyShaderSource = QStringLiteral(R"_(
#version 450

layout(local_size_x = 256) in;

layout(std430, binding = 0) readonly buffer SrcBuf { uint src_words[]; };
layout(std430, binding = 1) buffer DstBuf { uint dst_words[]; };
layout(std140, binding = 2) uniform CopyParams {
    uint count;
    uint element_words;
    uint src_offset_words;
    uint src_stride_words;
    uint dst_offset_words;
    uint dst_stride_words;
    // Workgroups along X, for a dispatch spread across Y past the backend's
    // per-dimension limit.
    uint num_workgroups_x;
    uint pad;
};

void main()
{
    // One invocation per copied word.
    uint i = gl_GlobalInvocationID.y * num_workgroups_x * gl_WorkGroupSize.x
             + gl_GlobalInvocationID.x;
    if(i >= count * element_words)
        return;
    uint e = i / element_words;
    uint w = i - e * element_words;
    dst_words[dst_offset_words + e * dst_stride_words + w]
        = src_words[src_offset_words + e * src_stride_words + w];
}
)_");

bool GPUStridedCopy::eligible(const Params& p) noexcept
{
  if(!p.src || !p.dst || p.count == 0 || p.element_bytes == 0)
    return false;
  if(p.src->type() == QRhiBuffer::Dynamic || p.dst->type() == QRhiBuffer::Dynamic)
    return false;
  if(!(p.src->usage() & QRhiBuffer::StorageBuffer)
     || !(p.dst->usage() & QRhiBuffer::StorageBuffer))
    return false;
  if((p.element_bytes | p.src_offset_bytes | p.src_stride_bytes | p.dst_offset_bytes
      | p.dst_stride_bytes)
     % 4u)
    return false;
  // Last element must fit in both buffers.
  const uint64_t n = p.count - 1;
  const uint64_t src_end
      = uint64_t(p.src_offset_bytes) + n * p.src_stride_bytes + p.element_bytes;
  const uint64_t dst_end
      = uint64_t(p.dst_offset_bytes) + n * p.dst_stride_bytes + p.element_bytes;
  return src_end <= uint64_t(p.src->size()) && dst_end <= uint64_t(p.dst->size())
         && uint64_t(p.count) * (p.element_bytes / 4u) <= 0xFFFFFFFFull;
}

bool GPUStridedCopy::init(RenderState& state)
{
  QRhi& rhi = *state.rhi;
  if(!rhi.isFeatureSupported(QRhi::Compute))
    return false;

  if(const int maxDim = rhi.resourceLimit(QRhi::MaxThreadGroupsPerDimension); maxDim > 0)
    m_maxWorkgroupsPerDim = maxDim;

  try
  {
    m_shader = makeCompute(state, stridedCopyShaderSource);
  }
  catch(const std::exception& e)
  {
    qWarning() << "GPUStridedCopy: failed to compile shader:" << e.what();
    return false;
  }

  m_pipeline = rhi.newComputePipeline();
  m_pipeline->setName("GPUStridedCopy");
  m_pipeline->setShaderStage(QRhiShaderStage(QRhiShaderStage::Compute, m_shader));
  return true;
}

void GPUStridedCopy::release()
{
  delete m_pipeline;
  m_pipeline = nullptr;
}

static void setCopyBindings(
    QRhiShaderResourceBindings& srb, const GPUStridedCopy::Params& p, QRhiBuffer* ubo)
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

GPUStridedCopy::PreparedOp GPUStridedCopy::prepare(QRhi& rhi, const Params& p)
{
  PreparedOp op;
  op.paramsUBO = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 32);
  op.paramsUBO->setName("GPUStridedCopy::params");
  op.paramsUBO->create();

  op.srb = rhi.newShaderResourceBindings();
  op.srb->setName("GPUStridedCopy::srb");
  setCopyBindings(*op.srb, p, op.paramsUBO);
  op.srb->create();
  op.src_id = p.src->globalResourceId();
  op.dst_id = p.dst->globalResourceId();

  // The pipeline only reads the layout of this srb, at create(); dispatch()
  // always passes the op's own srb, so this one may be released first.
  if(m_pipeline && !m_pipeline->shaderResourceBindings())
  {
    m_pipeline->setShaderResourceBindings(op.srb);
    m_pipeline->create();
  }
  return op;
}

void GPUStridedCopy::releaseOp(PreparedOp& op)
{
  delete op.srb;
  op.srb = nullptr;
  delete op.paramsUBO;
  op.paramsUBO = nullptr;
}

GPUStridedCopy::Dims GPUStridedCopy::dims(uint64_t invocations) const noexcept
{
  const int64_t maxGroups = std::max(1, m_maxWorkgroupsPerDim);
  const int64_t groups = (int64_t(invocations) + LocalSize - 1) / LocalSize;
  if(groups <= 0)
    return {};
  if(groups <= maxGroups)
    return {int(groups), 1};
  return {int(maxGroups), int((groups + maxGroups - 1) / maxGroups)};
}

void GPUStridedCopy::updateParams(
    QRhiResourceUpdateBatch& res, PreparedOp& op, const Params& p)
{
  if(!op.paramsUBO || !op.srb)
    return;
  const uint32_t words = p.element_bytes / 4u;
  const uint32_t data[8]{
      p.count,
      words,
      p.src_offset_bytes / 4u,
      p.src_stride_bytes / 4u,
      p.dst_offset_bytes / 4u,
      p.dst_stride_bytes / 4u,
      uint32_t(dims(uint64_t(p.count) * words).x),
      0u};
  res.updateDynamicBuffer(op.paramsUBO, 0, sizeof(data), data);

  // The buffers may have been reallocated since the srb was created. Only
  // then rebuild it: create() allocates descriptor sets on Vulkan.
  const quint64 src_id = p.src->globalResourceId();
  const quint64 dst_id = p.dst->globalResourceId();
  if(src_id != op.src_id || dst_id != op.dst_id)
  {
    setCopyBindings(*op.srb, p, op.paramsUBO);
    op.srb->create();
    op.src_id = src_id;
    op.dst_id = dst_id;
  }
}

void GPUStridedCopy::dispatch(QRhiCommandBuffer& cb, const PreparedOp& op, const Params& p)
{
  if(!m_pipeline || !op.srb)
    return;
  const auto d = dims(uint64_t(p.count) * (p.element_bytes / 4u));
  if(d.x <= 0)
    return;
  cb.setComputePipeline(m_pipeline);
  cb.setShaderResources(op.srb);
  cb.dispatch(d.x, d.y, 1);
}

}
