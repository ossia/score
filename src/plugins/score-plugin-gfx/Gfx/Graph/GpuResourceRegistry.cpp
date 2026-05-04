#include <Gfx/Graph/GpuResourceRegistry.hpp>

#include <Gfx/Graph/CustomMesh.hpp>  // BUFTRACE
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>  // MaterialGPU layout

#include <score/tools/Debug.hpp>

#include <QDebug>
#include <QImage>

namespace score::gfx
{
namespace
{
// Per-arena configuration. Capacities are the Phase 1 hard cap; growth is
// a Phase 2 concern (allocate() logs + returns invalid Slot on overflow).
// Sizes are deliberately conservative — a typical scene has 1-6 cameras,
// 1-16 lights, 10-50 materials, 50-1000 draws. The caps below allow
// ~50× headroom before we need grow-in-place.
// Per-arena configuration: fixed-stride layout. Buffer capacity is
// stride × slot_count. Consumer shaders index arena.entries[slot_index]
// with std430 stride == slot_stride.
struct ArenaConfig
{
  uint32_t slot_stride;   // byte stride per slot
  uint32_t slot_count;    // number of slots
  QRhiBuffer::UsageFlags usage;
  QRhiBuffer::Type type;
  const char* name;
};

// Entry order MUST match the Arena enum in GpuResourceRegistry.hpp.
constexpr ArenaConfig kArenaConfigs[(std::size_t)GpuResourceRegistry::Arena::Count_]
    = {
        // RawCamera — 64 B stride × 32 slots = 2 KiB. UBO dynamic.
        {64, 32, QRhiBuffer::UniformBuffer, QRhiBuffer::Dynamic,
         "GpuResourceRegistry::raw_camera"},

        // RawLight — 64 B stride × 4096 slots = 256 KiB. SSBO static
        //   (QRhi forbids StorageBuffer + Dynamic). Sized for VJ /
        //   particle-driven workflows that emit thousands of procedural
        //   lights via pack_lights_from_points / wander_lights_inline /
        //   grid_lights_inline. Typical 3D-file scenes (a handful of
        //   scene-node lights) pay only for the first N used slots —
        //   the rest is dormant device-local memory, no per-frame
        //   upload cost. Keep in sync with ScenePreprocessor's
        //   lightIdxBytes floor (must be slot_count * 4 bytes).
        {64, 4096, QRhiBuffer::StorageBuffer, QRhiBuffer::Static,
         "GpuResourceRegistry::raw_light"},

        // RawTransform — 64 B stride × 16384 slots = 1 MiB. Sized for
        //   heavy glTF / FBX scenes with 5-10k nodes.
        {64, 16384, QRhiBuffer::StorageBuffer, QRhiBuffer::Static,
         "GpuResourceRegistry::raw_transform"},

        // Material — 80 B stride × 32768 slots = 2.5 MiB. Shader indexes
        //   this arena directly as scene_materials.entries[material_index].
        //   Sized for enterprise / architectural-scale USD content (city
        //   assemblies, CAD exports, Pixar Kitchen_set-class scenes) —
        //   those routinely pack 1k–20k unique materials across all their
        //   per-prop references. Small scenes pay only for the first N
        //   used slots; the rest is dormant SSBO space.
        {80, 32768, QRhiBuffer::StorageBuffer, QRhiBuffer::Static,
         "GpuResourceRegistry::material"},

        // Env — 64 B stride × 8 slots = 512 B. UBO dynamic.
        {64, 8, QRhiBuffer::UniformBuffer, QRhiBuffer::Dynamic,
         "GpuResourceRegistry::env"},
};

} // namespace

GpuResourceRegistry::~GpuResourceRegistry()
{
  destroy();
}

void GpuResourceRegistry::init(QRhi& rhi)
{
  SCORE_ASSERT(!m_rhi);
  m_rhi = &rhi;

  for(std::size_t i = 0; i < m_arenas.size(); ++i)
  {
    auto& a = m_arenas[i];
    const auto& cfg = kArenaConfigs[i];
    const uint32_t bytes = cfg.slot_stride * cfg.slot_count;

    a.buffer = rhi.newBuffer(cfg.type, cfg.usage, bytes);
    a.buffer->setName(cfg.name);
    if(!a.buffer->create())
    {
      qWarning() << "GpuResourceRegistry: failed to create arena buffer"
                 << cfg.name
                 << "— falling back to null (allocations will fail)";
      delete a.buffer;
      a.buffer = nullptr;
      continue;
    }

    a.slot_stride = cfg.slot_stride;
    a.slot_count = cfg.slot_count;
    a.usage = cfg.usage;
    a.type = cfg.type;
    // Generation table sized to slot_count. Start at 1 so a freshly-
    // default gpu_slot_ref (generation=0) never matches a real slot.
    a.slot_generations.assign(cfg.slot_count, 1u);
    // Free-list stack: push slots in reverse order so pop yields slot
    // index 0, 1, 2, ... in allocation order. Keeps the arena buffer
    // densely packed at the front, which downstream tooling may assume.
    a.free_slots.clear();
    a.free_slots.reserve(cfg.slot_count);
    for(uint32_t s = cfg.slot_count; s-- > 0;)
      a.free_slots.push_back(s);
  }

  // Reserve Material arena slot 0 as the "default material" sentinel.
  // arenaSlotForMaterial(nullptr) returns 0; seedDefaults() writes a
  // white-dielectric MaterialGPU into that slot once a resource-update
  // batch is available. Pop from the free-list now so no producer can
  // claim it. (Other arenas keep slot 0 available — only Material has
  // the "null fallback" semantics.)
  {
    auto& mat = m_arenas[(std::size_t)Arena::Material];
    if(!mat.free_slots.empty() && mat.free_slots.back() == 0u)
      mat.free_slots.pop_back();
  }

  // Mesh arena — one QRhiBuffer per attribute stream, plus TWO shared
  // OffsetAllocators (vertex-units and index-units). See the
  // "CRITICAL invariant" block in GpuResourceRegistry.hpp for why the
  // allocators are NOT per-stream: a single baseVertex applies to all
  // vertex bindings, so per-mesh byte offsets across streams must be
  // proportional to per-stream stride. One allocator → one logical
  // vertex slot → guaranteed lockstep.
  for(std::size_t i = 0; i < m_meshStreams.size(); ++i)
  {
    auto& s = m_meshStreams[i];
    const uint32_t bytes = kMeshCapBytes[i];

    using UF = QRhiBuffer::UsageFlags;
    UF usage;
    if(i == (std::size_t)MeshStream::Indices)
      usage = UF(QRhiBuffer::IndexBuffer);
    else
      usage = UF(QRhiBuffer::VertexBuffer | QRhiBuffer::StorageBuffer);

    s.buffer = rhi.newBuffer(QRhiBuffer::Static, usage, bytes);
    const char* names[(std::size_t)MeshStream::Count_] = {
        "MeshArena::positions", "MeshArena::normals",
        "MeshArena::texcoords", "MeshArena::tangents",
        "MeshArena::colors",    "MeshArena::texcoords1",
        "MeshArena::indices"};
    s.buffer->setName(names[i]);
    if(!s.buffer->create())
    {
      qWarning() << "GpuResourceRegistry: failed to create mesh arena stream"
                 << names[i] << "— acquireMeshSlab will return null.";
      delete s.buffer;
      s.buffer = nullptr;
      continue;
    }
    s.capacity_bytes = bytes;
    s.usage = usage;
  }

  // Shared vertex/index allocators. Capacity in SLOTS, not bytes.
  // For vertex slots: every vertex stream must accommodate
  // capacity_slots × its_stride bytes. The min over the four vertex
  // streams determines the safe cap.
  uint32_t vertSlotCap = 0xFFFFFFFFu;
  for(std::size_t i = 0; i < (std::size_t)MeshStream::Indices; ++i)
  {
    if(!m_meshStreams[i].buffer)
    {
      vertSlotCap = 0;
      break;
    }
    vertSlotCap = std::min(
        vertSlotCap, m_meshStreams[i].capacity_bytes / kMeshStride[i]);
  }
  m_vertexSlotsCapacity = vertSlotCap;
  if(vertSlotCap > 0)
  {
    m_vertexAllocator = std::make_unique<OffsetAllocator::Allocator>(
        vertSlotCap, 128u * 1024u);
  }

  const auto& idxStream = m_meshStreams[(std::size_t)MeshStream::Indices];
  m_indexSlotsCapacity = idxStream.buffer
      ? idxStream.capacity_bytes
            / kMeshStride[(std::size_t)MeshStream::Indices]
      : 0u;
  if(m_indexSlotsCapacity > 0)
  {
    m_indexAllocator = std::make_unique<OffsetAllocator::Allocator>(
        m_indexSlotsCapacity, 128u * 1024u);
  }

  m_vertexSlotsUsed = 0;
  m_indexSlotsUsed = 0;
}

void GpuResourceRegistry::seedDefaults(QRhiResourceUpdateBatch& batch)
{
  if(m_defaults_seeded)
    return;

  // Material arena slot 0 — the default material returned by
  // arenaSlotForMaterial(nullptr). MaterialGPU's in-class initializers
  // are exactly the right defaults (white baseColor, metallic=0,
  // roughness=0.5, occlusion=1, no emissive, all texture refs null), so
  // a default-constructed instance is the byte payload we want.
  auto& mat = m_arenas[(std::size_t)Arena::Material];
  if(mat.buffer && mat.slot_stride >= sizeof(MaterialGPU))
  {
    MaterialGPU defaultMat{};
    batch.uploadStaticBuffer(
        mat.buffer, /*offset=*/0,
        (quint32)sizeof(MaterialGPU), &defaultMat);
  }

  m_defaults_seeded = true;
}

void GpuResourceRegistry::destroy(RenderList& renderer)
{
  // Route every arena buffer release through RenderList::releaseBuffer
  // so the RenderList's bookkeeping sees the release and the buffer is
  // destroyed through the same code path as every other QRhiBuffer in
  // the pipeline.
  for(auto& a : m_arenas)
  {
    if(a.buffer)
    {
      renderer.releaseBuffer(a.buffer);
      a.buffer = nullptr;
    }
    a.slot_stride = 0;
    a.slot_count = 0;
    for(auto& g : a.slot_generations)
      ++g;
    a.slot_generations.clear();
    a.free_slots.clear();
  }
  m_defaults_seeded = false;
  for(auto& ch : m_textureChannels)
  {
    for(auto& b : ch.buckets)
    {
      if(b.array)
      {
        b.array->deleteLater();
        b.array = nullptr;
      }
      if(b.sampler)
      {
        b.sampler->deleteLater();
        b.sampler = nullptr;
      }
      b.layers = 0;
      b.layerMap.clear();
    }
    ch.buckets.clear();
    ch.dynamicSlotMap.clear();
    ch.dynamicTextures.clear();
  }
  // Mesh arena teardown. Route through releaseBuffer (same invariant
  // as the component arenas) so downstream MeshBuffers that still
  // reference one of our slab offsets don't hit use-after-free.
  for(auto& s : m_meshStreams)
  {
    if(s.buffer)
    {
      renderer.releaseBuffer(s.buffer);
      s.buffer = nullptr;
    }
    s.capacity_bytes = 0;
  }
  m_vertexAllocator.reset();
  m_indexAllocator.reset();
  m_vertexSlotsCapacity = 0;
  m_indexSlotsCapacity = 0;
  m_vertexSlotsUsed = 0;
  m_indexSlotsUsed = 0;
  m_meshSlabs.clear();
  m_pendingReleases.clear();
  m_rhi = nullptr;
}

void GpuResourceRegistry::destroy()
{
  // Destructor fallback — nulls the buffer pointers without touching
  // the QRhi. Safe when destroy(RenderList&) already ran; leaks the
  // QRhiBuffer wrapper if QRhi has been torn down without a prior
  // RenderList-routed release (deleteLater on a dangling buffer would
  // crash, and leaking the wrapper is the lesser evil).
  for(auto& a : m_arenas)
  {
    a.buffer = nullptr;
    a.slot_stride = 0;
    a.slot_count = 0;
    for(auto& g : a.slot_generations)
      ++g;
    a.slot_generations.clear();
    a.free_slots.clear();
  }
  m_defaults_seeded = false;
  for(auto& ch : m_textureChannels)
  {
    // Do NOT deleteLater on textures here — if QRhi has already been
    // torn down their storage is gone. Leak the wrapper, same rule
    // as arena buffers above.
    for(auto& b : ch.buckets)
    {
      b.array = nullptr;
      b.sampler = nullptr;
      b.layers = 0;
      b.layerMap.clear();
    }
    ch.buckets.clear();
    ch.dynamicSlotMap.clear();
    ch.dynamicTextures.clear();
  }
  // Mesh arena: null the buffers (leaking the wrappers, same rule);
  // tear down allocators since those are pure CPU-side.
  for(auto& s : m_meshStreams)
  {
    s.buffer = nullptr;
    s.capacity_bytes = 0;
  }
  m_vertexAllocator.reset();
  m_indexAllocator.reset();
  m_vertexSlotsCapacity = 0;
  m_indexSlotsCapacity = 0;
  m_vertexSlotsUsed = 0;
  m_indexSlotsUsed = 0;
  m_meshSlabs.clear();
  m_pendingReleases.clear();
  m_rhi = nullptr;
}

const char* GpuResourceRegistry::textureChannelArrayName(TextureChannel ch) noexcept
{
  switch(ch)
  {
    case TextureChannel::BaseColor:  return "baseColorArray";
    case TextureChannel::MetalRough: return "metalRoughArray";
    case TextureChannel::Normal:     return "normalArray";
    case TextureChannel::Emissive:   return "emissiveArray";
    case TextureChannel::Occlusion:  return "occlusionArray";
    default:                         return "";
  }
}

const char* GpuResourceRegistry::textureChannelDynBaseName(TextureChannel ch) noexcept
{
  switch(ch)
  {
    case TextureChannel::BaseColor:  return "baseColorDyn";
    case TextureChannel::MetalRough: return "metalRoughDyn";
    case TextureChannel::Normal:     return "normalDyn";
    case TextureChannel::Emissive:   return "emissiveDyn";
    case TextureChannel::Occlusion:  return "occlusionDyn";
    default:                         return "";
  }
}

QRhiTexture::Flags GpuResourceRegistry::textureChannelFlags(TextureChannel ch) noexcept
{
  switch(ch)
  {
    case TextureChannel::BaseColor:
    case TextureChannel::Emissive:
      return QRhiTexture::sRGB;
    // Occlusion is a single-channel data texture (R = occlusion). Linear,
    // not sRGB. RGBA8 for now (we use only the R channel) — a future
    // optimisation could route to R8 to save VRAM.
    default:
      return {};
  }
}

int GpuResourceRegistry::resolveStaticLayer(
    TextureChannel channel, const ossia::texture_source* src,
    QRhiResourceUpdateBatch& /*res*/) noexcept
{
  if(!src)
    return -1;
  auto& ch = textureChannel(channel);
  // Wave 1: exactly one bucket live. Walk buckets[] and return the
  // first match; Wave 2 will return a (bucket, layer) pair instead.
  for(const auto& b : ch.buckets)
  {
    auto it = b.layerMap.find(src);
    if(it != b.layerMap.end())
      return it->second;
  }
  // Lazy-upload of newly-encountered sources is preprocessor-owned
  // for now (it has the QImage decode helper + the list-of-all-
  // materials context to size the array correctly). Producers calling
  // into the registry during their own update() accept -1 for an
  // as-yet-unregistered source; the preprocessor's next rebuildChannel()
  // pass will decode + register + patch refs for them. Once
  // decodeTextureSource moves here, this branch becomes the
  // decode + newTextureArray + uploadTexture path.
  return -1;
}

int GpuResourceRegistry::resolveDynamicSlot(
    TextureChannel channel, void* native_handle) noexcept
{
  if(!native_handle)
    return -1;
  auto& ch = textureChannel(channel);
  auto it = ch.dynamicSlotMap.find(native_handle);
  if(it != ch.dynamicSlotMap.end())
    return it->second;
  if((int)ch.dynamicTextures.size() >= kMaxDynamicSlots)
    return -1;
  const int slot = (int)ch.dynamicTextures.size();
  ch.dynamicSlotMap[native_handle] = slot;
  ch.dynamicTextures.push_back(static_cast<QRhiTexture*>(native_handle));
  return slot;
}


GpuResourceRegistry::Slot GpuResourceRegistry::allocate(Arena arena, uint32_t size)
{
  Slot slot;
  slot.arena = arena;
  slot.size = size;

  auto& a = m_arenas[(std::size_t)arena];
  if(!a.buffer || a.slot_stride == 0)
  {
    qWarning() << "GpuResourceRegistry::allocate: arena"
               << (int)arena << "is not initialised";
    return slot;
  }
  if(size > a.slot_stride)
  {
    qWarning() << "GpuResourceRegistry::allocate: requested size"
               << size << "exceeds arena"
               << kArenaConfigs[(std::size_t)arena].name << "stride"
               << a.slot_stride;
    return slot;
  }
  if(a.free_slots.empty())
  {
    qWarning() << "GpuResourceRegistry::allocate: arena"
               << kArenaConfigs[(std::size_t)arena].name
               << "is full — all" << a.slot_count << "slots in use";
    return slot;
  }
  slot.slot_index = a.free_slots.back();
  a.free_slots.pop_back();
  // Bump and stamp the generation. Any gpu_slot_ref still holding the
  // previous generation for this slot index will fail isLive().
  slot.generation = ++a.slot_generations[slot.slot_index];
  return slot;
}

void GpuResourceRegistry::free(Slot& slot)
{
  if(!slot.valid())
    return;
  auto& a = m_arenas[(std::size_t)slot.arena];
  if(slot.slot_index < a.slot_generations.size())
  {
    // Bump the generation first so any dangling ref from this Slot
    // fails isLive() regardless of whether the slot gets re-allocated.
    ++a.slot_generations[slot.slot_index];
    a.free_slots.push_back(slot.slot_index);
  }
  slot.slot_index = Slot::kInvalidIndex;
  slot.generation = 0;
}

QRhiBuffer* GpuResourceRegistry::buffer(Arena arena) const noexcept
{
  return m_arenas[(std::size_t)arena].buffer;
}

uint32_t GpuResourceRegistry::slotOffset(const Slot& slot) const noexcept
{
  if(!slot.valid())
    return 0u;
  return slot.slot_index * m_arenas[(std::size_t)slot.arena].slot_stride;
}

uint32_t GpuResourceRegistry::arenaSlotStride(Arena arena) const noexcept
{
  return m_arenas[(std::size_t)arena].slot_stride;
}

uint32_t GpuResourceRegistry::arenaSlotCount(Arena arena) const noexcept
{
  return m_arenas[(std::size_t)arena].slot_count;
}

void GpuResourceRegistry::updateSlot(
    QRhiResourceUpdateBatch& res, const Slot& slot, const void* data,
    uint32_t size) noexcept
{
  if(!slot.valid() || !data || size == 0)
    return;
  auto& a = m_arenas[(std::size_t)slot.arena];
  if(!a.buffer)
    return;

  const uint32_t offset = slotOffset(slot);
  SCORE_ASSERT(offset + size <= a.slot_stride * a.slot_count);

  if(a.type == QRhiBuffer::Dynamic)
    res.updateDynamicBuffer(a.buffer, offset, size, data);
  else
    res.uploadStaticBuffer(a.buffer, offset, size, data);
}

// ─── Mesh arena manager ──────────────────────────────────────────

GpuResourceRegistry::MeshSlab* GpuResourceRegistry::acquireMeshSlab(
    uint64_t stable_id, uint32_t vertex_count, uint32_t index_count) noexcept
{
  if(stable_id == 0)
    return nullptr;  // caller without stable_id — skip slab caching

  // Fast path: existing slab, same counts. Zero-cost hit.
  auto it = m_meshSlabs.find(stable_id);
  if(it != m_meshSlabs.end())
  {
    auto& slab = it->second;
    if(slab.vertex_count == vertex_count && slab.index_count == index_count)
    {
      slab.freshly_allocated = false;
      return &slab;
    }
    // Count mismatch — same mesh primitive re-emitting with different
    // counts. Defer the free to the grace queue so an in-flight draw
    // referencing the old offset doesn't read freed-and-reused bytes.
    PendingRelease pr;
    pr.stable_id = stable_id;
    // released_frame=0 is fine here: the next sweep at frame F will see
    // 0 + grace <= F (true once F >= grace), which gives the same
    // ≥grace-frames safety. We don't have current_frame in scope.
    pr.released_frame = 0;
    pr.vertex_slot = slab.vertex_slot;
    pr.index_slot = slab.index_slot;
    m_pendingReleases.push_back(pr);
    m_meshSlabs.erase(it);
  }

  if(!m_vertexAllocator || !m_indexAllocator)
    return nullptr;

  // Fresh allocation. ONE vertex slot (in vertex units) shared by
  // positions/normals/texcoords/tangents, ONE index slot.
  MeshSlab slab;
  slab.stable_id = stable_id;
  slab.vertex_count = vertex_count;
  slab.index_count = index_count;
  slab.freshly_allocated = true;

  if(vertex_count > 0)
  {
    slab.vertex_slot = m_vertexAllocator->allocate(vertex_count);
    if(slab.vertex_slot.offset == OffsetAllocator::Allocation::NO_SPACE)
    {
      qWarning() << "GpuResourceRegistry::acquireMeshSlab: vertex pool "
                    "exhausted (requested"
                 << vertex_count << "verts; free"
                 << m_vertexAllocator->storageReport().totalFreeSpace
                 << "vertex slots). Skipping mesh stable_id="
                 << qulonglong(stable_id);
      return nullptr;
    }
    m_vertexSlotsUsed += vertex_count;
  }
  BUFTRACE() << "[MeshSlab] alloc id=" << qulonglong(stable_id)
             << " vc=" << vertex_count << " ic=" << index_count
             << " vSlot=" << slab.vertex_slot.offset
             << " (used=" << m_vertexSlotsUsed << "/" << m_vertexSlotsCapacity
             << ")";

  if(index_count > 0)
  {
    slab.index_slot = m_indexAllocator->allocate(index_count);
    if(slab.index_slot.offset == OffsetAllocator::Allocation::NO_SPACE)
    {
      qWarning() << "GpuResourceRegistry::acquireMeshSlab: index pool "
                    "exhausted (requested"
                 << index_count << "indices; free"
                 << m_indexAllocator->storageReport().totalFreeSpace
                 << "index slots). Skipping mesh stable_id="
                 << qulonglong(stable_id);
      // Roll back the vertex allocation we just made.
      if(vertex_count > 0
         && slab.vertex_slot.metadata != OffsetAllocator::Allocation::NO_SPACE)
      {
        m_vertexAllocator->free(slab.vertex_slot);
        if(m_vertexSlotsUsed >= vertex_count)
          m_vertexSlotsUsed -= vertex_count;
      }
      return nullptr;
    }
    m_indexSlotsUsed += index_count;
  }

  const auto [inserted_it, ok] = m_meshSlabs.emplace(stable_id, slab);
  return ok ? &inserted_it->second : nullptr;
}

void GpuResourceRegistry::markMeshSlabSeen(
    uint64_t stable_id, uint32_t current_frame) noexcept
{
  auto it = m_meshSlabs.find(stable_id);
  if(it != m_meshSlabs.end())
    it->second.last_seen_frame = current_frame;
}

void GpuResourceRegistry::sweepMeshSlabs(
    uint32_t current_frame, uint32_t grace) noexcept
{
  // Two-phase: move slabs past their grace into m_pendingReleases
  // (carrying their vertex+index Allocations), then process already-
  // pending releases whose grace has elapsed and actually free from
  // the OffsetAllocators.
  //
  // The grace period guards against use-after-free: an
  // indirect_draw_cmds entry issued last frame may still reference
  // the slab's byte offset through an in-flight draw on the GPU.
  // Waiting `grace >= FramesInFlight + 1` frames ensures the GPU is
  // done with it.
  for(auto it = m_meshSlabs.begin(); it != m_meshSlabs.end();)
  {
    // Underflow-safe comparison: if current_frame is less than grace,
    // nothing is old enough yet.
    if(current_frame >= grace
       && it->second.last_seen_frame + grace <= current_frame)
    {
      PendingRelease pr;
      pr.stable_id = it->first;
      pr.released_frame = current_frame;
      pr.vertex_slot = it->second.vertex_slot;
      pr.index_slot = it->second.index_slot;
      m_pendingReleases.push_back(pr);
      it = m_meshSlabs.erase(it);
    }
    else
    {
      ++it;
    }
  }

  // Process the grace queue: anything submitted at least `grace`
  // frames ago is safely freed from the OffsetAllocator now.
  for(auto it = m_pendingReleases.begin(); it != m_pendingReleases.end();)
  {
    if(current_frame >= grace
       && it->released_frame + grace <= current_frame)
    {
      BUFTRACE() << "[MeshSlab] free  id=" << qulonglong(it->stable_id)
                 << " vSlot=" << it->vertex_slot.offset
                 << " iSlot=" << it->index_slot.offset
                 << " released_at=" << it->released_frame
                 << " current=" << current_frame;
      if(m_vertexAllocator
         && it->vertex_slot.metadata != OffsetAllocator::Allocation::NO_SPACE)
      {
        const auto sz
            = m_vertexAllocator->allocationSize(it->vertex_slot);
        m_vertexAllocator->free(it->vertex_slot);
        if(m_vertexSlotsUsed >= sz) m_vertexSlotsUsed -= sz;
      }
      if(m_indexAllocator
         && it->index_slot.metadata != OffsetAllocator::Allocation::NO_SPACE)
      {
        const auto sz
            = m_indexAllocator->allocationSize(it->index_slot);
        m_indexAllocator->free(it->index_slot);
        if(m_indexSlotsUsed >= sz) m_indexSlotsUsed -= sz;
      }
      it = m_pendingReleases.erase(it);
    }
    else
    {
      ++it;
    }
  }
}

void GpuResourceRegistry::releaseMeshSlab(uint64_t stable_id) noexcept
{
  auto it = m_meshSlabs.find(stable_id);
  if(it == m_meshSlabs.end())
    return;
  auto& slab = it->second;
  if(m_vertexAllocator
     && slab.vertex_slot.metadata != OffsetAllocator::Allocation::NO_SPACE)
  {
    const auto sz = m_vertexAllocator->allocationSize(slab.vertex_slot);
    m_vertexAllocator->free(slab.vertex_slot);
    if(m_vertexSlotsUsed >= sz) m_vertexSlotsUsed -= sz;
  }
  if(m_indexAllocator
     && slab.index_slot.metadata != OffsetAllocator::Allocation::NO_SPACE)
  {
    const auto sz = m_indexAllocator->allocationSize(slab.index_slot);
    m_indexAllocator->free(slab.index_slot);
    if(m_indexSlotsUsed >= sz) m_indexSlotsUsed -= sz;
  }
  m_meshSlabs.erase(it);
}

uint32_t GpuResourceRegistry::meshSlabOffsetBytes(
    const MeshSlab& slab, MeshStream stream) const noexcept
{
  // Single source of truth for per-stream byte offsets:
  //   vertex streams → vertex_slot.offset (in vertex units) × stride
  //   index  stream  → index_slot.offset  (in index  units) × 4
  // Independent allocators per stream would let these diverge, which
  // would silently produce wrong attribute reads under fragmentation.
  if(stream == MeshStream::Indices)
    return slab.index_slot.offset
        * kMeshStride[(std::size_t)MeshStream::Indices];
  return slab.vertex_slot.offset * kMeshStride[(std::size_t)stream];
}

QRhiBuffer* GpuResourceRegistry::meshStreamBuffer(MeshStream s) const noexcept
{
  return m_meshStreams[(std::size_t)s].buffer;
}

void GpuResourceRegistry::uploadMeshStream(
    QRhiResourceUpdateBatch& res, const MeshSlab& slab,
    MeshStream s, const void* data, uint32_t size) noexcept
{
  auto& stream = m_meshStreams[(std::size_t)s];
  if(!stream.buffer || !data || size == 0)
    return;
  const uint32_t offset = meshSlabOffsetBytes(slab, s);
  // Guard against out-of-bounds writes. Slab capacity in bytes:
  //   vertex streams: vertex_count × stride
  //   index  stream:  index_count  × 4
  const uint32_t slot_capacity_bytes
      = (s == MeshStream::Indices)
          ? slab.index_count * kMeshStride[(std::size_t)MeshStream::Indices]
          : slab.vertex_count * kMeshStride[(std::size_t)s];
  if(size > slot_capacity_bytes)
  {
    qWarning() << "GpuResourceRegistry::uploadMeshStream: upload" << size
               << "bytes exceeds slab capacity" << slot_capacity_bytes
               << "(stream" << (int)s << ")";
    return;
  }
  if(offset + size > stream.capacity_bytes)
  {
    qWarning() << "GpuResourceRegistry::uploadMeshStream: upload offset+size"
               << (offset + size) << "exceeds stream capacity"
               << stream.capacity_bytes << "(stream" << (int)s << ")";
    return;
  }
  res.uploadStaticBuffer(stream.buffer, offset, size, data);
}

uint32_t GpuResourceRegistry::meshStreamUsedBytes(MeshStream s) const noexcept
{
  if(s == MeshStream::Indices)
    return m_indexSlotsUsed * kMeshStride[(std::size_t)MeshStream::Indices];
  return m_vertexSlotsUsed * kMeshStride[(std::size_t)s];
}

uint32_t GpuResourceRegistry::meshStreamFreeBytes(MeshStream s) const noexcept
{
  if(s == MeshStream::Indices)
  {
    if(!m_indexAllocator) return 0u;
    return m_indexAllocator->storageReport().totalFreeSpace
        * kMeshStride[(std::size_t)MeshStream::Indices];
  }
  if(!m_vertexAllocator) return 0u;
  return m_vertexAllocator->storageReport().totalFreeSpace
      * kMeshStride[(std::size_t)s];
}

} // namespace score::gfx
