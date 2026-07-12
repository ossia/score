#pragma once

#include <Gfx/Graph/VertexFallbackDefaults.hpp>

#include <ossia/detail/hash_map.hpp>

#include <score_plugin_gfx_export.h>

#include <cstdint>

class QRhi;
class QRhiBuffer;
class QRhiResourceUpdateBatch;

namespace score::gfx
{

// Shared pool of tiny (4–16 byte) PerInstance step_rate=1 vertex
// buffers used to satisfy "REQUIRED: false" VERTEX_INPUTS whose
// upstream geometry does not provide a matching attribute.
//
// Lifetime-owned by the RenderList (same scope as GpuResourceRegistry).
// Lookup key includes the format, stride, and a hash of the payload so
// different DEFAULT values on the same semantic don't share a buffer.
// A typical session touches ~5–10 distinct buckets; total footprint is
// sub-kilobyte.
//
// Not thread-safe: designed for single-threaded render-thread access.
class SCORE_PLUGIN_GFX_EXPORT VertexFallbackPool
{
public:
  struct Entry
  {
    QRhiBuffer* buffer{};   // VertexBuffer | Immutable, exactly `stride` bytes
    uint32_t stride{};      // matches spec.stride_bytes
    int format{};           // matches spec.format (ossia::geometry::attribute::format)
  };

  VertexFallbackPool() = default;
  ~VertexFallbackPool();

  VertexFallbackPool(const VertexFallbackPool&) = delete;
  VertexFallbackPool& operator=(const VertexFallbackPool&) = delete;

  // Returns (and lazily creates) the shared buffer matching `spec`.
  // The first call per key allocates a QRhiBuffer and records an
  // upload on `batch`; subsequent calls return the cached buffer and
  // do not touch `batch`.
  //
  // `rhi` and `batch` must be valid. The returned buffer is valid
  // until release() is called.
  Entry acquire(QRhi& rhi, QRhiResourceUpdateBatch& batch,
                const VertexFallbackSpec& spec);

  // Destroy every cached buffer and clear the pool. Called by the
  // owning RenderList on teardown.
  void release();

  // Diagnostic only.
  std::size_t size() const noexcept { return m_entries.size(); }

private:
  struct Key
  {
    int format{};
    uint32_t stride{};
    uint64_t payload_hash{};

    bool operator==(const Key& o) const noexcept
    {
      return format == o.format && stride == o.stride
             && payload_hash == o.payload_hash;
    }
  };
  struct KeyHash
  {
    std::size_t operator()(const Key& k) const noexcept
    {
      // Cheap mix — keys are already high-entropy via payload_hash.
      return (std::size_t)(k.payload_hash
                           ^ ((uint64_t)k.format << 32)
                           ^ (uint64_t)k.stride);
    }
  };

  ossia::hash_map<Key, Entry, KeyHash> m_entries;
};

} // namespace score::gfx
