#include "IndirectCountClamp.hpp"

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/RhiComputeBarrier.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <QDebug>

#include <private/qrhi_p.h>

namespace score::gfx
{
namespace
{
// Every word of a live slot is copied, every word of a dead one zeroed: a
// zero vertex / index and instance count draws nothing, whichever of the two
// command layouts the producer wrote.
static const constexpr auto clampShaderSource = R"_(#version 450
layout(local_size_x = 64) in;

layout(std140, binding = 0) uniform Params
{
  uint slots;
  uint commandsOffset; // words
  uint stride;         // words
  uint countOffset;    // words
};
layout(std430, binding = 1) readonly buffer Commands { uint commands[]; };
layout(std430, binding = 2) readonly buffer Count { uint drawCount[]; };
layout(std430, binding = 3) writeonly buffer Clamped { uint clamped[]; };

void main()
{
  const uint slot = gl_GlobalInvocationID.x;
  if(slot >= slots)
    return;
  const bool live = slot < drawCount[countOffset];
  for(uint w = 0u; w < stride; w++)
    clamped[slot * stride + w] = live ? commands[commandsOffset + slot * stride + w] : 0u;
}
)_";

bool storageUsable(const QRhiBuffer* buf)
{
  return buf && buf->usage().testFlag(QRhiBuffer::StorageBuffer)
         && buf->type() != QRhiBuffer::Dynamic;
}
}

bool IndirectCountClamp::ensurePipeline(RenderState& state)
{
  if(m_pipeline)
    return true;
  if(m_failed)
    return false;
  QRhi& rhi = *state.rhi;
  if(!rhi.isFeatureSupported(QRhi::Compute))
  {
    m_failed = true;
    return false;
  }
  try
  {
    m_pipeline = rhi.newComputePipeline();
    m_pipeline->setShaderStage(
        QRhiShaderStage(QRhiShaderStage::Compute, makeCompute(state, clampShaderSource)));
  }
  catch(const std::exception& e)
  {
    qWarning() << "IndirectCountClamp: failed to compile shader:" << e.what();
    delete m_pipeline;
    m_pipeline = nullptr;
    m_failed = true;
    return false;
  }
  m_params = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 16);
  m_params->setName("IndirectCountClamp_params");
  m_params->create();
  return true;
}

QRhiBuffer* IndirectCountClamp::run(
    RenderState& state, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch*& res,
    const Input& in)
{
  m_refusal = nullptr;
  auto refuse = [this](const char* why) -> QRhiBuffer* {
    m_refusal = why;
    return nullptr;
  };
  if(!in.commands || !in.count || in.slots == 0 || in.stride == 0
     || (in.stride % 4) != 0 || (in.commandsOffset % 4) != 0
     || (in.countOffset % 4) != 0)
    return refuse("the command or count layout is not word-aligned");
  if(!storageUsable(in.commands) || !storageUsable(in.count))
    return refuse("the command or count buffer is not a storage buffer");
  if(RenderList::isRetiringBuffer(in.commands) || RenderList::isRetiringBuffer(in.count))
    return refuse("the command or count buffer is being retired");
  const quint64 needed = quint64(in.commandsOffset) + quint64(in.slots) * in.stride;
  if(needed > quint64(in.commands->size())
     || quint64(in.countOffset) + 4 > quint64(in.count->size()))
    return refuse("the command or count buffer is smaller than its slots");
  const quint64 groups = (quint64(in.slots) + LocalSize - 1) / LocalSize;
  const int maxGroups = state.rhi->resourceLimit(QRhi::MaxThreadGroupsPerDimension);
  if(maxGroups > 0 && groups > quint64(maxGroups))
    return refuse("there are more command slots than one dispatch covers");
  if(!ensurePipeline(state))
    return refuse("compute is unavailable");

  QRhi& rhi = *state.rhi;
  const quint32 clampedSize = in.slots * in.stride;
  if(m_clamped && m_clamped->size() < clampedSize)
  {
    delete m_srb;
    m_srb = nullptr;
    m_clamped->destroy();
    m_clamped->setSize(clampedSize);
    if(!m_clamped->create())
      return refuse("the clamped command buffer could not be created");
  }
  if(!m_clamped)
  {
    m_clamped = rhi.newBuffer(
        QRhiBuffer::Static,
        QRhiBuffer::StorageBuffer | QRhiBuffer::IndirectBuffer, clampedSize);
    m_clamped->setName("IndirectCountClamp_commands");
    if(!m_clamped->create())
    {
      delete m_clamped;
      m_clamped = nullptr;
      return refuse("the clamped command buffer could not be created");
    }
  }

  // By resource id, not address: a new buffer can land at a freed one's
  // address. A producer may also recreate its buffers in place: compare the
  // sizes too.
  if(!m_srb || m_boundCommands != in.commands->globalResourceId()
     || m_boundCount != in.count->globalResourceId()
     || m_boundCommandsSize != in.commands->size()
     || m_boundCountSize != in.count->size())
  {
    delete m_srb;
    using B = QRhiShaderResourceBinding;
    m_srb = rhi.newShaderResourceBindings();
    m_srb->setBindings({
        B::uniformBuffer(0, B::ComputeStage, m_params),
        B::bufferLoad(1, B::ComputeStage, in.commands),
        B::bufferLoad(2, B::ComputeStage, in.count),
        B::bufferStore(3, B::ComputeStage, m_clamped),
    });
    if(!m_srb->create())
    {
      delete m_srb;
      m_srb = nullptr;
      return refuse("the clamp bindings could not be created");
    }
    m_boundCommands = in.commands->globalResourceId();
    m_boundCount = in.count->globalResourceId();
    m_boundCommandsSize = in.commands->size();
    m_boundCountSize = in.count->size();
    if(!m_pipeline->shaderResourceBindings())
    {
      m_pipeline->setShaderResourceBindings(m_srb);
      if(!m_pipeline->create())
      {
        qWarning() << "IndirectCountClamp: the clamp pipeline could not be created";
        release();
        m_failed = true;
        return refuse("compute is unavailable");
      }
    }
  }

  if(!res)
    res = rhi.nextResourceUpdateBatch();
  const quint32 params[4]
      = {in.slots, in.commandsOffset / 4, in.stride / 4, in.countOffset / 4};
  res->updateDynamicBuffer(m_params, 0, sizeof(params), params);

  // The producer's writes reach this pass by its own barrier (see
  // RenderedCSFNode); on GL this pass orders its own writes the same way.
  const bool gl = rhi.backend() == QRhi::OpenGLES2;
  if(gl)
    cb.beginComputePass(res, QRhiCommandBuffer::BeginPassFlag::ExternalContent);
  else
    cb.beginComputePass(res);
  res = nullptr;
  cb.setComputePipeline(m_pipeline);
  cb.setShaderResources(m_srb);
  cb.dispatch(int(groups), 1, 1);
  if(gl)
  {
    cb.beginExternal();
    insertComputeBarrier(rhi, cb);
    cb.endExternal();
  }
  cb.endComputePass();
  return m_clamped;
}

void IndirectCountClamp::release()
{
  delete m_srb;
  m_srb = nullptr;
  delete m_pipeline;
  m_pipeline = nullptr;
  delete m_params;
  m_params = nullptr;
  delete m_clamped;
  m_clamped = nullptr;
  m_boundCommands = 0;
  m_boundCount = 0;
  m_boundCommandsSize = 0;
  m_boundCountSize = 0;
}
}
