#include "InjectTexture.hpp"

#include <algorithm>

namespace Threedim
{

void InjectTexture::rebuild()
{
  const auto& in = inputs.scene_in.scene;
  const ossia::scene_state* in_state = in.state.get();
  const int64_t in_version = in_state ? in_state->version : -1;
  void* cur_handle = inputs.texture.texture.handle;
  const auto& cur_name = inputs.aux_name.value;

  m_cached_in_state = in_state;
  m_cached_in_version = in_version;
  m_cached_handle = cur_handle;
  m_cached_name = cur_name;

  if(!cur_handle || cur_name.empty() || !in_state)
  {
    m_cached_out = in.state;
    m_pending_dirty = 0xFF;
    return;
  }

  auto state = std::make_shared<ossia::scene_state>(*in_state);
  state->inject_textures.erase(
      std::remove_if(
          state->inject_textures.begin(), state->inject_textures.end(),
          [&](const ossia::aux_inject_texture& at) { return at.name == cur_name; }),
      state->inject_textures.end());
  state->inject_textures.push_back(
      {.name = cur_name, .native_handle = cur_handle});
  state->version = ++m_version_counter;
  state->dirty_index = m_version_counter;

  m_cached_out = state;
  m_pending_dirty = 0xFF;
}

void InjectTexture::operator()()
{
  // Upstream scene_state + live texture handle can change mid-stream;
  // detect and rebuild.
  const auto* in_state = inputs.scene_in.scene.state.get();
  const int64_t in_version = in_state ? in_state->version : -1;
  void* cur_handle = inputs.texture.texture.handle;
  const bool upstream_changed
      = m_cached_in_state != in_state
        || m_cached_in_version != in_version
        || m_cached_handle != cur_handle;
  if(!m_cached_out || upstream_changed)
    rebuild();
  outputs.scene_out.scene.state = m_cached_out;
  outputs.scene_out.dirty = m_pending_dirty;
  m_pending_dirty = 0;
}

} // namespace Threedim
