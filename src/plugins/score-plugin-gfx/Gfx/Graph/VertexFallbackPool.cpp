#include <Gfx/Graph/VertexFallbackPool.hpp>

#include <private/qrhi_p.h>

#include <algorithm>
#include <cstring>
#include <vector>

namespace score::gfx
{

VertexFallbackPool::~VertexFallbackPool()
{
  // RenderList owns us and must have called release() before
  // tearing down the QRhi. Anything still in the map at destruction
  // time would leak — but we can't safely delete QRhiBuffer* here
  // without knowing the QRhi is still alive, so we just assert the
  // caller did the right thing via an empty-map check.
  // (Destructive assert would fire during OOM teardown; leave it as
  // a quiet leak for robustness.)
}

bool VertexFallbackPool::grow(
    QRhi& rhi, QRhiResourceUpdateBatch& batch, Record& rec, uint32_t instances)
{
  const uint32_t stride = rec.spec.stride_bytes;
  if(stride == 0 || stride > rec.spec.bytes.size())
    return false;

  // The floor covers draws whose instance count never reaches the CPU;
  // see VertexFallbackPool::default_instances.
  const uint32_t want = std::max(instances, default_instances);
  if(rec.entry.buffer && rec.entry.instances >= want)
    return true;

  const quint32 bytes = (quint32)stride * (quint32)want;

  // Replicate the constant `want` times: the binding is PerInstance
  // step_rate=1, so instance i fetches element i and every element must
  // carry the same value.
  std::vector<uint8_t> payload((std::size_t)bytes);
  for(uint32_t i = 0; i < want; i++)
    std::memcpy(payload.data() + (std::size_t)i * stride, rec.spec.bytes.data(), stride);

  const uint32_t prev_instances = rec.entry.instances;
  if(!rec.entry.buffer)
  {
    // The Immutable usage hint means QRhi uploads once and never
    // touches the backing memory again.
    auto* buf = rhi.newBuffer(
        QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, bytes);
    buf->setName(QByteArrayLiteral("score.vertex_fallback"));
    if(!buf->create())
    {
      // Allocation failed. Leave the entry null; the caller propagates
      // it as a pipeline-build failure.
      delete buf;
      return false;
    }
    rec.entry.buffer = buf;
  }
  else
  {
    // Resize the SAME QRhiResource rather than allocating a new one:
    // FallbackBindingPlan slots cache the QRhiBuffer* across frames, so
    // the handle has to survive the growth. destroy() on a QRhiResource
    // defers the native release until the frames still referencing it
    // have retired.
    rec.entry.buffer->destroy();
    rec.entry.buffer->setSize(bytes);
    if(!rec.entry.buffer->create())
    {
      // Out of memory at the larger size. Put the buffer back the way it
      // was so the draw keeps the coverage it had instead of binding a
      // destroyed buffer; if even that fails there is nothing usable
      // left, so drop the handle.
      rec.entry.buffer->setSize((quint32)stride * (quint32)prev_instances);
      if(prev_instances == 0 || !rec.entry.buffer->create())
      {
        rec.entry.buffer->deleteLater();
        rec.entry.buffer = nullptr;
        rec.entry.instances = 0;
        return false;
      }
      batch.uploadStaticBuffer(
          rec.entry.buffer, 0, (quint32)stride * (quint32)prev_instances,
          payload.data());
      return false;
    }
  }

  batch.uploadStaticBuffer(rec.entry.buffer, 0, bytes, payload.data());
  rec.entry.stride = stride;
  rec.entry.format = rec.spec.format;
  rec.entry.instances = want;
  return true;
}

VertexFallbackPool::Entry VertexFallbackPool::acquire(
    QRhi& rhi, QRhiResourceUpdateBatch& batch,
    const VertexFallbackSpec& spec, uint32_t instance_count)
{
  Key k{
      .format = spec.format,
      .stride = spec.stride_bytes,
      .payload_hash = hashVertexFallback(spec)};

  auto it = m_entries.find(k);
  if(it == m_entries.end())
    it = m_entries.emplace(k, Record{.entry = {}, .spec = spec}).first;

  // Creates on the first call for this key, grows on a later call that
  // needs more instances than the cached buffer covers, no-ops otherwise.
  grow(rhi, batch, it->second, instance_count);
  return it->second.entry;
}

bool VertexFallbackPool::ensureInstances(
    QRhi& rhi, QRhiResourceUpdateBatch& batch, QRhiBuffer* buffer,
    uint32_t instance_count)
{
  if(!buffer)
    return false;

  // Linear scan: a session holds ~5-10 entries and this runs once per
  // pass per frame, so the reverse index would cost more than it saves.
  for(auto& [k, rec] : m_entries)
    if(rec.entry.buffer == buffer)
      return grow(rhi, batch, rec, instance_count);
  return false;
}

void VertexFallbackPool::release()
{
  for(auto& [k, rec] : m_entries)
  {
    if(rec.entry.buffer)
    {
      rec.entry.buffer->deleteLater();
      rec.entry.buffer = nullptr;
    }
  }
  m_entries.clear();
}

} // namespace score::gfx
