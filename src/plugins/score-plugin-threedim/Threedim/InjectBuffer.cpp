#include "InjectBuffer.hpp"

#include <algorithm>

namespace Threedim
{

void InjectBuffer::rebuild()
{
  const auto& in = inputs.scene_in.scene;
  const ossia::scene_state* in_state = in.state.get();
  const int64_t in_version = in_state ? in_state->version : -1;
  void* cur_handle = inputs.buffer.buffer.handle;
  const int64_t cur_bytes = inputs.buffer.buffer.byte_size;
  const auto& cur_name = inputs.aux_name.value;

  m_cached_in_state = in_state;
  m_cached_in_version = in_version;
  m_cached_handle = cur_handle;
  m_cached_byte_size = cur_bytes;
  m_cached_name = cur_name;

  // Unwired / incomplete controls → pass-through. Safe to drop in a
  // pipeline before the Buffer is connected.
  if(!cur_handle || cur_name.empty() || !in_state)
  {
    m_cached_out = in.state;
    m_pending_dirty = 0xFF;
    return;
  }

  // Clone the scene_state (cheap — it's shallow pointers to shared
  // sub-vectors) and append the injection. Existing entries with the
  // same name are removed first so a later InjectBuffer in the chain
  // always wins.
  auto state = std::make_shared<ossia::scene_state>(*in_state);
  state->inject_buffers.erase(
      std::remove_if(
          state->inject_buffers.begin(), state->inject_buffers.end(),
          [&](const ossia::aux_inject_buffer& ab) { return ab.name == cur_name; }),
      state->inject_buffers.end());
  state->inject_buffers.push_back(
      {.name = cur_name,
       .native_handle = cur_handle,
       .byte_size = cur_bytes});
  state->version = ++m_version_counter;
  state->dirty_index = m_version_counter;

  m_cached_out = state;
  m_pending_dirty = 0xFF;
}

void InjectBuffer::operator()()
{
  // Upstream scene_state + live buffer handle can change without a
  // port-update event; detect and trigger rebuild. aux_name changes
  // come via the control update() callback.
  const auto* in_state = inputs.scene_in.scene.state.get();
  const int64_t in_version = in_state ? in_state->version : -1;
  void* cur_handle = inputs.buffer.buffer.handle;
  const int64_t cur_bytes = inputs.buffer.buffer.byte_size;
  const bool upstream_changed
      = m_cached_in_state != in_state
        || m_cached_in_version != in_version
        || m_cached_handle != cur_handle
        || m_cached_byte_size != cur_bytes;
  if(!m_cached_out || upstream_changed)
    rebuild();
  outputs.scene_out.scene.state = m_cached_out;
  outputs.scene_out.dirty = m_pending_dirty;
  m_pending_dirty = 0;
}

} // namespace Threedim
