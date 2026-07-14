#include "TagAs.hpp"

#include "PrimitiveCloud/FormatOverride.hpp"

namespace Threedim
{

void TagAs::rebuild()
{
  const auto& in = inputs.scene_in.scene;
  const ossia::scene_state* in_state = in.state.get();
  const int64_t in_version = in_state ? in_state->version : -1;
  const auto& cur_format = inputs.format_id.value;

  m_cached_in_state = in_state;
  m_cached_in_version = in_version;
  m_cached_format_id = cur_format;

  if(!in_state)
  {
    m_cached_out = in.state;
    m_pending_dirty = 0xFF;
    return;
  }

  // applyFormatOverride is the same helper AssetLoader uses, with the
  // same passthrough-when-empty contract. Returns the input verbatim
  // when format_id is empty so wiring stays cheap during edits.
  m_cached_out = Threedim::PrimitiveCloud::applyFormatOverride(
      in.state, cur_format);
  m_pending_dirty = 0xFF;
}

void TagAs::operator()()
{
  // The upstream scene_state ptr / version can change without a
  // port-update event (e.g. when a producer republishes the same
  // shared_ptr after an internal mutation). Detect and rebuild.
  const auto* in_state = inputs.scene_in.scene.state.get();
  const int64_t in_version = in_state ? in_state->version : -1;
  const bool upstream_changed
      = m_cached_in_state != in_state
        || m_cached_in_version != in_version;
  if(!m_cached_out || upstream_changed)
    rebuild();

  outputs.scene_out.scene.state = m_cached_out;
  outputs.scene_out.dirty = m_pending_dirty;
  m_pending_dirty = 0;
}

} // namespace Threedim
