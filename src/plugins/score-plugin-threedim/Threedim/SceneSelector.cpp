#include "SceneSelector.hpp"

#include <algorithm>

namespace Threedim
{

namespace
{

// Duplicated glob matcher; tiny, cheaper than adding a shared header.
bool selector_glob_match(std::string_view pattern, std::string_view text) noexcept
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

// DFS until the first match. Accumulates the found-node plus a hint
// whether the found node itself is the root of the subtree (so we
// know whether to apply the ZeroOut transform rebase).
ossia::scene_node_ptr selector_findByPath(
    const ossia::scene_node_ptr& n, std::string_view pat, const std::string& path)
{
  if(!n)
    return nullptr;
  if(selector_glob_match(pat, path))
    return n;
  if(!n->has_children())
    return nullptr;
  for(const auto& p : *n->children)
  {
    if(auto* sub = ossia::get_if<ossia::scene_node_ptr>(&p))
    {
      if(!*sub)
        continue;
      std::string childPath = path + '/' + (*sub)->name;
      if(auto r = selector_findByPath(*sub, pat, childPath))
        return r;
    }
  }
  return nullptr;
}

ossia::scene_node_ptr
findByName(const ossia::scene_node_ptr& n, std::string_view wanted)
{
  if(!n)
    return nullptr;
  if(n->name == wanted)
    return n;
  if(!n->has_children())
    return nullptr;
  for(const auto& p : *n->children)
  {
    if(auto* sub = ossia::get_if<ossia::scene_node_ptr>(&p))
      if(auto r = findByName(*sub, wanted))
        return r;
  }
  return nullptr;
}

// Strip the first scene_transform payload from a scene_node's children
// list — used for the ZeroOut rebase mode. The node layout convention
// (GltfParser / FbxParser / ConfigurePrimitive / etc.) puts the TRS
// as the first child payload; dropping it leaves the subtree at the
// world origin.
ossia::scene_node_ptr stripLeadingTransform(const ossia::scene_node_ptr& n)
{
  if(!n || !n->has_children())
    return n;
  if(n->children->empty())
    return n;
  if(!ossia::get_if<ossia::scene_transform>(&(*n->children)[0]))
    return n;

  auto clone_children
      = std::make_shared<std::vector<ossia::scene_payload>>(
          n->children->begin() + 1, n->children->end());
  auto copy = std::make_shared<ossia::scene_node>(*n);
  copy->children = std::move(clone_children);
  copy->dirty_index = n->dirty_index + 1;
  return copy;
}

} // namespace

void SceneSelector::rebuild()
{
  const auto& in = inputs.scene_in.scene;
  if(!in.state)
  {
    m_cached_out.reset();
    m_pending_dirty = 0;
    return;
  }

  const auto* s = in.state.get();
  const int64_t v = in.state->version;

  ossia::scene_node_ptr found;
  const auto mode = Mode(inputs.mode.value);
  if(in.state->roots)
  {
    switch(mode)
    {
      case ByIndex: {
        const auto idx = std::size_t(std::max(0, inputs.index.value));
        if(idx < in.state->roots->size())
          found = (*in.state->roots)[idx];
        break;
      }
      case ByName: {
        for(const auto& r : *in.state->roots)
        {
          if((found = findByName(r, inputs.path.value)))
            break;
        }
        break;
      }
      default: {
        for(const auto& r : *in.state->roots)
        {
          const std::string base = r ? ("/" + r->name) : std::string{};
          if((found = selector_findByPath(r, inputs.path.value, base)))
            break;
        }
        break;
      }
    }
  }

  if(found && inputs.rebase.value == ZeroOut)
    found = stripLeadingTransform(found);

  auto new_roots
      = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  if(found)
    new_roots->push_back(std::move(found));

  auto new_state = std::make_shared<ossia::scene_state>(*in.state);
  new_state->roots = std::move(new_roots);
  new_state->version = ++m_version_counter;
  new_state->dirty_index = in.state->dirty_index + 1;

  m_cached_out = std::move(new_state);
  m_cached_in_state = s;
  m_cached_in_version = v;
  m_pending_dirty = 0xFF;
}

void SceneSelector::operator()()
{
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
