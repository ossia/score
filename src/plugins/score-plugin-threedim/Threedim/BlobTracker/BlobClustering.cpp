#include "BlobClustering.hpp"

#include <Gfx/Graph/RenderList.hpp>

#include <QDebug>

#include <Threedim/GeometryToBufferStrategies.hpp>

namespace Threedim
{

Blobs::GpuPointSource BlobClustering::resolveSource() const noexcept
{
  const auto& mesh = inputs.geometry.mesh;
  if(mesh.vertices <= 0)
    return {};

  const auto lookup = findAttribute(mesh, halp::attribute_semantic::position);
  if(!lookup || !lookup->buffer || !lookup->buffer->handle)
    return {};

  // Only a float3/float4 position can be read as a vec3 straight out of the
  // vertex buffer; anything else would need a conversion pass in front, which
  // is Extract buffer's job, not this node's.
  const auto fmt = lookup->attribute->format;
  if(fmt != halp::attribute_format::float3 && fmt != halp::attribute_format::float4)
    return {};

  return Blobs::GpuPointSource{
      .buffer = static_cast<QRhiBuffer*>(lookup->buffer->handle),
      .count = mesh.vertices,
      .stride_bytes = lookup->binding->stride,
      .offset_bytes
      = lookup->attribute->byte_offset + (int32_t)lookup->input->byte_offset};
}

Blobs::ClusterParams BlobClustering::params() const noexcept
{
  Blobs::ClusterParams p;
  p.cluster_dist = inputs.cluster_dist.value;
  p.min_points = inputs.min_points.value;
  p.max_blobs = inputs.max_blobs.value;
  p.knn_k = inputs.knn_k.value;
  return p;
}

void BlobClustering::publish() noexcept
{
  auto* buf = m_runnable ? m_pipeline.results() : nullptr;
  if(!buf)
  {
    outputs.blobs.buffer = {};
    return;
  }

  outputs.blobs.buffer.handle = buf;
  outputs.blobs.buffer.byte_size = m_pipeline.results_bytes();
  outputs.blobs.buffer.byte_offset = 0;
}

void BlobClustering::init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  const auto src = resolveSource();
  if(!src.valid())
  {
    m_runnable = false;
    publish();
    return;
  }

  QRhi& rhi = *renderer.state.rhi;
  if(!m_pipeline.init(renderer.state, rhi, src.count, inputs.max_blobs.value))
  {
    m_runnable = false;
    publish();
    return;
  }

  m_runnable = m_pipeline.update(renderer.state, rhi, src, params());
  publish();
}

void BlobClustering::update(
    score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
    score::gfx::Edge* /*e*/)
{
  const auto src = resolveSource();

  // Drain the dirty flags so upstream knows the frame was picked up; the source
  // buffer handles are re-resolved every frame regardless, because a
  // reallocated vertex buffer would otherwise leave a dangling binding.
  inputs.geometry.dirty_mesh = false;
  for(auto& buf : inputs.geometry.mesh.buffers)
    buf.dirty = false;

  if(!src.valid())
  {
    m_runnable = false;
    publish();
    return;
  }

  QRhi& rhi = *renderer.state.rhi;
  if(!m_pipeline.initialized())
  {
    if(!m_pipeline.init(renderer.state, rhi, src.count, inputs.max_blobs.value))
    {
      m_runnable = false;
      publish();
      return;
    }
  }

  m_runnable = m_pipeline.update(renderer.state, rhi, src, params());
  publish();
}

void BlobClustering::release(score::gfx::RenderList& /*renderer*/)
{
  m_pipeline.release();
  m_runnable = false;
  publish();
}

void BlobClustering::runInitialPasses(
    score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
    QRhiResourceUpdateBatch*& res, score::gfx::Edge& /*edge*/)
{
  if(!m_runnable)
    return;
  m_pipeline.runCompute(*renderer.state.rhi, commands, res);
}

void BlobClustering::operator()() { }

}
