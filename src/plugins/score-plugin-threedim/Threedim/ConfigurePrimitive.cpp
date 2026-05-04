#include "ConfigurePrimitive.hpp"

#include <algorithm>

namespace Threedim
{

namespace
{

// Minimal glob matcher — shared logic with SceneGraphFilter.cpp, but
// duplicated here to avoid pulling that TU's anonymous-namespace
// contents. Move to a shared header if a third node needs it.
bool configure_glob_match(std::string_view pattern, std::string_view text) noexcept
{
  std::size_t pi = 0, ti = 0;
  std::size_t star_pi = std::string_view::npos;
  std::size_t star_ti = 0;
  bool star_double = false;

  while(ti < text.size())
  {
    if(pi < pattern.size())
    {
      char pc = pattern[pi];
      if(pc == '*')
      {
        star_double = (pi + 1 < pattern.size() && pattern[pi + 1] == '*');
        pi += star_double ? 2 : 1;
        star_pi = pi;
        star_ti = ti;
        continue;
      }
      if(pc == '?' && text[ti] != '/')
      {
        ++pi;
        ++ti;
        continue;
      }
      if(pc == text[ti])
      {
        ++pi;
        ++ti;
        continue;
      }
    }
    if(star_pi != std::string_view::npos)
    {
      if(!star_double && text[star_ti] == '/')
        return false;
      pi = star_pi;
      ++star_ti;
      ti = star_ti;
      continue;
    }
    return false;
  }
  while(pi < pattern.size() && pattern[pi] == '*')
    ++pi;
  return pi == pattern.size();
}

bool configure_any_match(
    const std::vector<std::string>& pats, std::string_view text) noexcept
{
  for(const auto& p : pats)
    if(configure_glob_match(p, text))
      return true;
  return false;
}

struct PrimitiveWalker
{
  ConfigurePrimitive::Mode mode;
  const std::vector<std::string>& paths;

  // Returns the updated node. Shares the original shared_ptr when no
  // descendant needed a change, so pointer identity is preserved for
  // un-touched branches (keeps downstream caches warm).
  ossia::scene_node_ptr
  rewrite(const ossia::scene_node_ptr& src, const std::string& path) const
  {
    if(!src)
      return src;

    const bool matches = configure_any_match(paths, path);
    bool need_self_update = false;
    bool new_active = src->active;
    bool new_visible = src->visible;

    if(matches)
    {
      switch(mode)
      {
        case ConfigurePrimitive::SetActive:
          new_active = true;
          break;
        case ConfigurePrimitive::SetInactive:
          new_active = false;
          break;
        case ConfigurePrimitive::SetVisible:
          new_visible = true;
          break;
        case ConfigurePrimitive::SetInvisible:
          new_visible = false;
          break;
        case ConfigurePrimitive::SetActiveAndVisible:
          new_active = true;
          new_visible = true;
          break;
        case ConfigurePrimitive::SetInactiveAndInvisible:
          new_active = false;
          new_visible = false;
          break;
      }
      need_self_update
          = (new_active != src->active) || (new_visible != src->visible);
    }

    if(!src->has_children())
    {
      if(!need_self_update)
        return src;
      auto copy = std::make_shared<ossia::scene_node>(*src);
      copy->active = new_active;
      copy->visible = new_visible;
      copy->dirty_index = src->dirty_index + 1;
      return copy;
    }

    auto new_children
        = std::make_shared<std::vector<ossia::scene_payload>>();
    new_children->reserve(src->children->size());
    bool any_child_changed = false;
    for(const auto& payload : *src->children)
    {
      if(auto* sub = ossia::get_if<ossia::scene_node_ptr>(&payload))
      {
        if(!*sub)
        {
          new_children->push_back(payload);
          continue;
        }
        std::string childPath = path + '/' + (*sub)->name;
        auto rw = rewrite(*sub, childPath);
        if(rw.get() != sub->get())
          any_child_changed = true;
        new_children->push_back(rw ? rw : *sub);
      }
      else
      {
        new_children->push_back(payload);
      }
    }

    if(!need_self_update && !any_child_changed)
      return src;

    auto copy = std::make_shared<ossia::scene_node>(*src);
    copy->active = new_active;
    copy->visible = new_visible;
    copy->children = std::move(new_children);
    copy->dirty_index = src->dirty_index + 1;
    return copy;
  }
};

} // namespace

void ConfigurePrimitive::rebuild()
{
  const auto& in = inputs.scene_in.scene;
  if(!in.state)
  {
    m_cached_out.reset();
    m_pending_dirty = 0;
    return;
  }

  const auto* in_state = in.state.get();
  const int64_t in_version = in.state->version;

  // Empty pattern list = no-op passthrough. Skip the walk entirely.
  if(inputs.paths.value.empty())
  {
    m_cached_out = in.state;
    m_cached_in_state = in_state;
    m_cached_in_version = in_version;
    m_cached_mode = inputs.mode.value;
    m_cached_paths = inputs.paths.value;
    m_pending_dirty = 0xFF;
    return;
  }

  PrimitiveWalker w{Mode(inputs.mode.value), inputs.paths.value};
  auto new_roots
      = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  if(in.state->roots)
  {
    new_roots->reserve(in.state->roots->size());
    for(const auto& r : *in.state->roots)
    {
      if(auto rw = w.rewrite(r, r ? ("/" + r->name) : std::string{}))
        new_roots->push_back(std::move(rw));
    }
  }

  auto new_state = std::make_shared<ossia::scene_state>(*in.state);
  new_state->roots = std::move(new_roots);
  new_state->version = ++m_version_counter;
  new_state->dirty_index = in.state->dirty_index + 1;

  m_cached_out = new_state;
  m_cached_in_state = in_state;
  m_cached_in_version = in_version;
  m_cached_mode = inputs.mode.value;
  m_cached_paths = inputs.paths.value;
  m_pending_dirty = 0xFF;
}

void ConfigurePrimitive::operator()()
{
  // Detect upstream scene_in pointer/version change and rebuild.
  // Control changes come through their update() callbacks.
  const auto* in_state = inputs.scene_in.scene.state.get();
  const int64_t in_version = in_state ? in_state->version : -1;
  const bool upstream_changed
      = m_cached_in_state != in_state || m_cached_in_version != in_version;
  if(upstream_changed || (!m_cached_out && in_state))
    rebuild();
  outputs.scene_out.scene.state = m_cached_out;
  outputs.scene_out.dirty = m_pending_dirty;
  m_pending_dirty = 0;
}

} // namespace Threedim
