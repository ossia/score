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

// Shared pool of tiny PerInstance step_rate=1 vertex buffers used to
// satisfy "REQUIRED: false" VERTEX_INPUTS whose upstream geometry does
// not provide a matching attribute.
//
// A step_rate=1 PerInstance binding ADVANCES one element per instance;
// it is not a broadcast. A buffer holding a single element therefore
// reaches instance 0 only -- every later instance steps past the end and
// fetches zero (SR1 in the 2026-09 review). Fixing that on the *binding*
// is not portable: stepRate != 1 needs QRhi::CustomInstanceStepRate, which
// OpenGL reports as false (qrhigles2.cpp:1732) -- and a stride of 0 fails
// Metal validation. So the constant is replicated inside the
// buffer instead -- an entry holds `Entry::instances` copies of the
// payload and instance i reads copy i. The pipeline's vertex input
// layout fixes stride and classification, NOT buffer length, so growing
// an entry never requires a pipeline rebuild.
//
// Lifetime-owned by the RenderList (same scope as GpuResourceRegistry).
// Lookup key includes the format, stride, and a hash of the payload so
// different DEFAULT values on the same semantic don't share a buffer.
// A typical session touches ~5–10 distinct buckets; at the replication
// floor that is tens of kilobytes, and a few hundred at the widest
// payload the spec can carry.
//
// Not thread-safe: designed for single-threaded render-thread access.
class SCORE_PLUGIN_GFX_EXPORT VertexFallbackPool
{
public:
  // Replication floor, in instances. A draw whose instance count the CPU
  // cannot see -- GPU-driven drawIndirect / drawIndirectCount, where the
  // count lives in a buffer the host never reads -- never gets to call
  // ensureInstances(), so every entry starts out covering this many
  // instances. Draws whose count IS known grow past it on demand.
  static constexpr uint32_t default_instances = 1024;

  struct Entry
  {
    QRhiBuffer* buffer{};  // VertexBuffer | Immutable, `instances * stride` bytes
    uint32_t stride{};      // matches spec.stride_bytes
    int format{};           // matches spec.format (ossia::geometry::attribute::format)
    uint32_t instances{};   // copies of the payload the buffer holds
  };

  VertexFallbackPool() = default;
  ~VertexFallbackPool();

  VertexFallbackPool(const VertexFallbackPool&) = delete;
  VertexFallbackPool& operator=(const VertexFallbackPool&) = delete;

  // Returns (and lazily creates) the shared buffer matching `spec`,
  // holding at least max(instance_count, default_instances) copies of
  // the payload. The first call per key allocates a QRhiBuffer and
  // records an upload on `batch`; subsequent calls return the cached
  // buffer, growing it first when this caller needs more instances than
  // the cached one already covers.
  //
  // `rhi` and `batch` must be valid. The returned buffer is valid
  // until release() is called.
  Entry acquire(QRhi& rhi, QRhiResourceUpdateBatch& batch,
                const VertexFallbackSpec& spec, uint32_t instance_count = 1);

  // Grow the pooled entry whose buffer is `buffer` so that it covers
  // `instance_count` instances, re-uploading the replicated payload.
  // Called from the render path once the draw's instance count is known.
  // The QRhiBuffer object is resized in place, so FallbackBindingPlan
  // slots that cached the pointer stay valid and no pipeline is rebuilt.
  //
  // Returns false when `buffer` is not one of ours, or when the
  // reallocation failed -- in which case the entry keeps the coverage it
  // already had.
  bool ensureInstances(QRhi& rhi, QRhiResourceUpdateBatch& batch,
                       QRhiBuffer* buffer, uint32_t instance_count);

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

  // The payload is kept alongside the entry: growing a buffer means
  // re-uploading the constant replicated N times, and the caller that
  // knows the instance count (the render path) no longer has the spec.
  struct Record
  {
    Entry entry;
    VertexFallbackSpec spec;
  };

  // Resize `rec`'s buffer to hold max(instances, default_instances)
  // copies of its payload, creating it if it does not exist yet.
  // No-op (returns true) when the buffer already covers that many.
  bool grow(QRhi& rhi, QRhiResourceUpdateBatch& batch, Record& rec,
            uint32_t instances);

  ossia::hash_map<Key, Record, KeyHash> m_entries;
};

} // namespace score::gfx
