#include "BufferGatherPass.hpp"

#include <Gfx/Graph/RhiComputeBarrier.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <QDebug>

#include <algorithm>
#include <bit>

namespace Ui::Monitor::Buffer
{
namespace
{
// The source is read as words and its bytes taken from them: elements need
// no alignment and components may be 1 or 2 bytes. BufferGather.hpp holds
// the same arithmetic for the CPU.
const constexpr auto gather_shader = R"_(#version 450
layout(local_size_x = 256) in;

layout(std140, binding = 0) uniform Params
{
  uint base;
  uint sample_stride;
  uint element_bytes;
  uint out_words;
  uint src_words;
  uint reduce_count;
  uint reduce_base;
  uint reduce_stride;
  uint type;
  uint components;
  uint component_bytes;
  uint pad;
};
layout(std430, binding = 1) readonly buffer Source { uint src[]; };
layout(std430, binding = 2) buffer Staging { uint header[16]; uint samples[]; };

shared uint s_lo[4];
shared uint s_hi[4];
shared uint s_bad;

uint word(uint w)
{
  return w < src_words ? src[w] : 0u;
}

uint bytesAt(uint b, uint n)
{
  const uint w = b >> 2u;
  const uint shift = (b & 3u) * 8u;
  uint v = word(w) >> shift;
  if(shift + n * 8u > 32u)
    v |= word(w + 1u) << (32u - shift);
  return n == 4u ? v : (v & ((1u << (n * 8u)) - 1u));
}

uint gathered(uint w)
{
  const uint b = w * 4u;
  if(element_bytes % 4u == 0u)
    return bytesAt(base + (b / element_bytes) * sample_stride + b % element_bytes, 4u);
  uint v = 0u;
  for(uint i = 0u; i < 4u; i++)
  {
    const uint at = b + i;
    v |= bytesAt(base + (at / element_bytes) * sample_stride + at % element_bytes, 1u)
         << (i * 8u);
  }
  return v;
}

float component(uint b)
{
  switch(type)
  {
    case 0u: return uintBitsToFloat(bytesAt(b, 4u));
    case 1u: return unpackHalf2x16(bytesAt(b, 2u)).x;
    case 2u: return float(int(bytesAt(b, 4u)));
    case 3u: return float(bytesAt(b, 4u));
    case 4u: return float(bytesAt(b, 1u)) / 255.0;
    default: return float(bytesAt(b, 1u));
  }
}

uint ordered(float f)
{
  const uint u = floatBitsToUint(f);
  return (u & 0x80000000u) != 0u ? ~u : (u | 0x80000000u);
}

void main()
{
  const uint threads = gl_NumWorkGroups.x * gl_NumWorkGroups.y * 256u;
  const uint id = gl_GlobalInvocationID.x + gl_GlobalInvocationID.y * gl_NumWorkGroups.x * 256u;
  for(uint w = id; w < out_words; w += threads)
    samples[w] = gathered(w);

  // The same for every invocation: none of them reaches the barriers
  if(reduce_count == 0u)
    return;

  if(gl_LocalInvocationIndex == 0u)
  {
    for(int c = 0; c < 4; c++)
    {
      s_lo[c] = 0xffffffffu;
      s_hi[c] = 0u;
    }
    s_bad = 0u;
  }
  memoryBarrierShared();
  barrier();

  uint lo[4] = uint[4](0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu);
  uint hi[4] = uint[4](0u, 0u, 0u, 0u);
  uint bad = 0u;
  for(uint i = id; i < reduce_count; i += threads)
  {
    const uint at = reduce_base + i * reduce_stride;
    for(uint c = 0u; c < components; c++)
    {
      const float v = component(at + c * component_bytes);
      if(isnan(v) || isinf(v))
      {
        bad++;
        continue;
      }
      const uint o = ordered(v);
      lo[c] = min(lo[c], o);
      hi[c] = max(hi[c], o);
    }
  }
  for(uint c = 0u; c < components; c++)
  {
    atomicMin(s_lo[c], lo[c]);
    atomicMax(s_hi[c], hi[c]);
  }
  if(bad > 0u)
    atomicAdd(s_bad, bad);
  memoryBarrierShared();
  barrier();

  if(gl_LocalInvocationIndex == 0u)
  {
    for(uint c = 0u; c < components; c++)
    {
      atomicMin(header[c], s_lo[c]);
      atomicMax(header[4u + c], s_hi[c]);
    }
    atomicAdd(header[8], s_bad);
  }
}
)_";
}

GatherPass::~GatherPass()
{
  release();
}

Fallback GatherPass::check(QRhi& rhi, const QRhiBuffer& source) const noexcept
{
  if(!rhi.isFeatureSupported(QRhi::Compute))
    return Fallback::NoCompute;
  if(!source.usage().testFlag(QRhiBuffer::StorageBuffer)
     || source.type() == QRhiBuffer::Dynamic)
    return Fallback::NotStorage;
  if(m_failed)
    return Fallback::Failed;
  return Fallback::None;
}

bool GatherPass::ensurePipeline(score::gfx::RenderState& state)
{
  if(m_pipeline)
    return true;
  if(m_failed)
    return false;
  auto& rhi = *state.rhi;
  try
  {
    m_shader = score::gfx::makeCompute(state, gather_shader);
  }
  catch(const std::exception& e)
  {
    qWarning() << "Buffer view: the gather shader did not compile:" << e.what();
    m_failed = true;
    return false;
  }
  m_params = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(GatherParams));
  m_params->setName("BufferView::gather_params");
  if(!m_params->create())
  {
    m_failed = true;
    return false;
  }
  m_pipeline = rhi.newComputePipeline();
  m_pipeline->setShaderStage({QRhiShaderStage::Compute, m_shader});
  return true;
}

bool GatherPass::ensureStaging(QRhi& rhi, int64_t bytes)
{
  if(m_staging && int64_t(m_staging->size()) >= bytes)
    return true;
  // Grown by powers of two, to what one readback can hold at most
  const int64_t size = std::min<int64_t>(
      std::bit_ceil(uint64_t(std::max<int64_t>(bytes, 4096))),
      max_readback_bytes + gather_header_bytes);
  if(size < bytes)
    return false;
  delete m_staging;
  m_staging = rhi.newBuffer(QRhiBuffer::Static, QRhiBuffer::StorageBuffer, quint32(size));
  m_staging->setName("BufferView::gather_staging");
  if(!m_staging->create())
  {
    delete m_staging;
    m_staging = nullptr;
    return false;
  }
  return true;
}

bool GatherPass::ensureBindings(QRhi& rhi, QRhiBuffer& source, const GatherPlan& plan)
{
  if(m_srb && m_sourceId == source.globalResourceId()
     && m_stagingId == m_staging->globalResourceId() && m_bindOffset == plan.bind_offset
     && m_bindSize == plan.bind_size)
    return true;
  delete m_srb;
  m_srb = rhi.newShaderResourceBindings();
  m_srb->setBindings(
      {QRhiShaderResourceBinding::uniformBuffer(
           0, QRhiShaderResourceBinding::ComputeStage, m_params),
       QRhiShaderResourceBinding::bufferLoad(
           1, QRhiShaderResourceBinding::ComputeStage, &source, quint32(plan.bind_offset),
           quint32(plan.bind_size)),
       QRhiShaderResourceBinding::bufferLoadStore(
           2, QRhiShaderResourceBinding::ComputeStage, m_staging)});
  if(!m_srb->create())
  {
    delete m_srb;
    m_srb = nullptr;
    return false;
  }
  m_sourceId = source.globalResourceId();
  m_stagingId = m_staging->globalResourceId();
  m_bindOffset = plan.bind_offset;
  m_bindSize = plan.bind_size;
  if(!m_pipeline->shaderResourceBindings())
  {
    m_pipeline->setShaderResourceBindings(m_srb);
    if(!m_pipeline->create())
    {
      qWarning() << "Buffer view: the gather pipeline could not be created";
      delete m_pipeline;
      m_pipeline = nullptr;
      m_failed = true;
      return false;
    }
  }
  return true;
}

bool GatherPass::record(
    score::gfx::RenderState& state, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch*& res,
    QRhiBuffer& source, const GatherPlan& plan, QRhiBufferReadbackResult& out)
{
  if(!plan.valid || !state.rhi)
    return false;
  auto& rhi = *state.rhi;
  if(!ensurePipeline(state) || !ensureStaging(rhi, plan.staging_bytes)
     || !ensureBindings(rhi, source, plan))
    return false;

  auto* readback = rhi.nextResourceUpdateBatch();
  if(!readback)
    return false;
  if(!res)
    res = rhi.nextResourceUpdateBatch();
  if(!res)
  {
    readback->release();
    return false;
  }
  res->updateDynamicBuffer(m_params, 0, sizeof(GatherParams), &plan.params);
  if(plan.params.reduce_count > 0)
  {
    uint32_t reset[gather_header_words]{};
    std::fill_n(reset, 4, 0xffffffffu);
    res->uploadStaticBuffer(m_staging, 0, sizeof(reset), reset);
  }

  const bool gl = rhi.backend() == QRhi::OpenGLES2;
  cb.beginComputePass(
      res, gl ? QRhiCommandBuffer::BeginPassFlag::ExternalContent
              : QRhiCommandBuffer::BeginPassFlags{});
  res = nullptr;
  cb.setComputePipeline(m_pipeline);
  cb.setShaderResources(m_srb);
  cb.dispatch(plan.groups_x, plan.groups_y, 1);
  if(gl)
  {
    cb.beginExternal();
    score::gfx::insertComputeBarrier(rhi, cb);
    cb.endExternal();
  }
  readback->readBackBuffer(
      m_staging, quint32(plan.readback_offset), quint32(plan.readback_size), &out);
  cb.endComputePass(readback);
  return true;
}

void GatherPass::release() noexcept
{
  delete m_pipeline;
  delete m_srb;
  delete m_params;
  delete m_staging;
  m_pipeline = nullptr;
  m_srb = nullptr;
  m_params = nullptr;
  m_staging = nullptr;
  m_sourceId = 0;
  m_stagingId = 0;
  m_bindOffset = -1;
  m_bindSize = -1;
  m_shader = {};
  m_failed = false;
}
}
