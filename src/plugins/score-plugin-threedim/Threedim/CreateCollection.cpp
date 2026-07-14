#include "CreateCollection.hpp"

namespace Threedim
{

void CreateCollection::rebuild()
{
  const auto& in = inputs.scene_in.scene;
  const ossia::scene_state* in_state = in.state.get();
  const int64_t in_version = in_state ? in_state->version : -1;

  const auto& cur_name = inputs.name.value;
  const auto& cur_paths = inputs.paths.value;
  const auto& cur_tags = inputs.tags.value;

  m_cached_in_state = in_state;
  m_cached_in_version = in_version;
  m_cached_name = cur_name;
  m_cached_paths = cur_paths;
  m_cached_tags = cur_tags;

  // An empty name is a no-op — pass the input through so the node is
  // safe to wire in even before the user fills in the Name field.
  if(cur_name.empty() || cur_paths.empty())
  {
    m_cached_out = in.state;
    m_pending_dirty = 0xFF;
    return;
  }

  auto coll = std::make_shared<ossia::scene_collection>();
  coll->name = cur_name;
  for(const auto& p : cur_paths)
    coll->paths.push_back(p);
  for(const auto& t : cur_tags)
    coll->tags.push_back(t);

  // Rebuild the collections vector: copy existing entries whose name
  // doesn't collide with ours (overwriting duplicates keeps the
  // interaction model simple — each CreateCollection "owns" its name),
  // then append the new one.
  auto merged = std::make_shared<std::vector<ossia::scene_collection_ptr>>();
  if(in_state && in_state->collections)
  {
    for(const auto& c : *in_state->collections)
      if(c && c->name != cur_name)
        merged->push_back(c);
  }
  merged->push_back(std::move(coll));

  auto state = std::make_shared<ossia::scene_state>();
  if(in_state)
  {
    state->roots = in_state->roots;
    state->materials = in_state->materials;
    state->animations = in_state->animations;
    state->cameras = in_state->cameras;
    state->skeletons = in_state->skeletons;
    state->environment = in_state->environment;
    state->active_camera_id = in_state->active_camera_id;
  }
  state->collections = std::move(merged);
  state->version = ++m_version_counter;
  state->dirty_index = m_version_counter;

  m_cached_out = state;
  m_pending_dirty = 0xFF;
}

void CreateCollection::operator()()
{
  const auto* in_state = inputs.scene_in.scene.state.get();
  const int64_t in_version = in_state ? in_state->version : -1;
  const bool upstream_changed
      = m_cached_in_state != in_state || m_cached_in_version != in_version;
  if(!m_cached_out || upstream_changed)
    rebuild();
  outputs.scene_out.scene.state = m_cached_out;
  outputs.scene_out.dirty = m_pending_dirty;
  m_pending_dirty = 0;
}

} // namespace Threedim
