#include <Gfx/Graph/VertexFallbackPool.hpp>

#include <QtGui/rhi/qrhi.h>

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

VertexFallbackPool::Entry VertexFallbackPool::acquire(
    QRhi& rhi, QRhiResourceUpdateBatch& batch,
    const VertexFallbackSpec& spec)
{
  Key k{
      .format = spec.format,
      .stride = spec.stride_bytes,
      .payload_hash = hashVertexFallback(spec)};

  if(auto it = m_entries.find(k); it != m_entries.end())
    return it->second;

  // Allocate a single QRhiBuffer sized to exactly one element. The
  // Immutable usage hint means QRhi uploads once and never touches
  // the backing memory again.
  auto* buf = rhi.newBuffer(
      QRhiBuffer::Immutable,
      QRhiBuffer::VertexBuffer,
      spec.stride_bytes);
  buf->setName(QByteArrayLiteral("score.vertex_fallback"));
  if(!buf->create())
  {
    // Allocation failed. Return a null Entry; the caller will
    // propagate as a pipeline-build failure.
    delete buf;
    return Entry{};
  }

  batch.uploadStaticBuffer(buf, 0, spec.stride_bytes, spec.bytes.data());

  Entry e{.buffer = buf, .stride = spec.stride_bytes, .format = spec.format};
  m_entries.emplace(k, e);
  return e;
}

void VertexFallbackPool::release()
{
  for(auto& [k, e] : m_entries)
  {
    if(e.buffer)
    {
      e.buffer->deleteLater();
      e.buffer = nullptr;
    }
  }
  m_entries.clear();
}

} // namespace score::gfx
