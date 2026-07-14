#include "ExtractSceneBuffer.hpp"

#include <Gfx/Graph/RenderList.hpp>

namespace Threedim
{

namespace
{
// Resolve the slot ref from the scene + kind + index selectors. Returns
// an all-zero (invalid) ref on miss; the caller's isLive() call will
// reject it without a separate null check.
inline ossia::gpu_slot_ref
pickSlotRef(const ossia::scene_state& state, ExtractSceneBuffer::Kind kind,
            int index) noexcept
{
  switch(kind)
  {
    case ExtractSceneBuffer::Environment:
      return state.environment.raw_slot;

    case ExtractSceneBuffer::Camera:
    {
      if(!state.cameras)
        return {};
      const auto& cams = *state.cameras;
      if(index < 0 || std::size_t(index) >= cams.size())
        return {};
      if(!cams[index])
        return {};
      return cams[index]->raw_slot;
    }

    case ExtractSceneBuffer::Material:
    {
      if(!state.materials)
        return {};
      const auto& mats = *state.materials;
      if(index < 0 || std::size_t(index) >= mats.size())
        return {};
      if(!mats[index])
        return {};
      return mats[index]->raw_slot;
    }
  }
  return {};
}

inline score::gfx::GpuResourceRegistry::Arena arenaOf(uint32_t tag) noexcept
{
  return static_cast<score::gfx::GpuResourceRegistry::Arena>(tag);
}
}

void ExtractSceneBuffer::operator()()
{
  // Execution thread — no GPU work here. The port's scene_spec is what
  // update() reads. Drain the dirty flag so upstream knows the tick
  // was observed.
  inputs.scene_in.dirty = 0;
}

void ExtractSceneBuffer::init(
    score::gfx::RenderList&, QRhiResourceUpdateBatch&)
{
  // Nothing to allocate — the node only reads through the registry.
  outputs.buffer.buffer = {};
}

void ExtractSceneBuffer::update(
    score::gfx::RenderList& renderer, QRhiResourceUpdateBatch&,
    score::gfx::Edge*)
{
  // No scene → clear outlet. Downstream consumers see buffer.handle ==
  // nullptr and fall back to whatever default they define.
  if(!inputs.scene_in.scene.state)
  {
    outputs.buffer.buffer = {};
    return;
  }

  const auto ref = pickSlotRef(
      *inputs.scene_in.scene.state,
      Kind(inputs.kind.value), inputs.index.value);

  // Liveness is the one authoritative check: catches stale refs
  // (producer released), default-constructed refs (no slot stamped),
  // refs from a different registry (different RenderList), and
  // mismatched-arena refs in one compare.
  if(!renderer.registry().isLive(ref))
  {
    outputs.buffer.buffer = {};
    return;
  }

  QRhiBuffer* buf = renderer.registry().buffer(arenaOf(ref.arena));
  if(!buf)
  {
    outputs.buffer.buffer = {};
    return;
  }

  const void* prev_handle = outputs.buffer.buffer.handle;
  const int64_t prev_offset = outputs.buffer.buffer.byte_offset;
  const int64_t prev_size = outputs.buffer.buffer.byte_size;

  outputs.buffer.buffer.handle = buf;
  outputs.buffer.buffer.byte_offset = (int64_t)ref.offset;
  outputs.buffer.buffer.byte_size = (int64_t)ref.size;
  // Flip `changed` only when something downstream-observable actually
  // moved — most frames the slot is stable and we want downstream
  // rebinds to short-circuit on identity.
  outputs.buffer.buffer.changed
      = (prev_handle != buf)
        || (prev_offset != (int64_t)ref.offset)
        || (prev_size != (int64_t)ref.size);
}

void ExtractSceneBuffer::release(score::gfx::RenderList&)
{
  outputs.buffer.buffer = {};
}

}
