#include "FormatOverride.hpp"

#include <ossia/detail/variant.hpp>

#include <functional>
#include <utility>

namespace Threedim::PrimitiveCloud
{

namespace
{

// Recursively rewrites primitive_cloud_components inside a scene_node's
// children list. Returns a fresh scene_node shared_ptr when something
// was rewritten (or a nested scene_node was rewritten), the original
// otherwise — so unchanged subtrees keep their identity for downstream
// fingerprinting.
ossia::scene_node_ptr rewriteNode(
    const ossia::scene_node_ptr& in, std::string_view override)
{
  if(!in || !in->children || in->children->empty())
    return in;

  bool any_rewrite = false;
  std::vector<ossia::scene_payload> fresh_children;
  fresh_children.reserve(in->children->size());

  for(const auto& payload : *in->children)
  {
    if(auto* pc = ossia::get_if<ossia::primitive_cloud_component_ptr>(&payload))
    {
      if(*pc && (*pc)->format_id != override)
      {
        auto fresh = std::make_shared<ossia::primitive_cloud_component>(**pc);
        fresh->format_id = std::string{override};
        fresh_children.emplace_back(
            ossia::primitive_cloud_component_ptr{std::move(fresh)});
        any_rewrite = true;
        continue;
      }
    }
    else if(auto* sn = ossia::get_if<ossia::scene_node_ptr>(&payload))
    {
      auto rewritten = rewriteNode(*sn, override);
      if(rewritten.get() != sn->get())
      {
        fresh_children.emplace_back(std::move(rewritten));
        any_rewrite = true;
        continue;
      }
    }
    fresh_children.emplace_back(payload);
  }

  if(!any_rewrite)
    return in;

  auto fresh = std::make_shared<ossia::scene_node>(*in);
  fresh->children = std::make_shared<std::vector<ossia::scene_payload>>(
      std::move(fresh_children));
  return fresh;
}

} // namespace

std::shared_ptr<ossia::scene_state> applyFormatOverride(
    std::shared_ptr<const ossia::scene_state> state, std::string_view override)
{
  if(!state)
    return nullptr;
  if(override.empty())
    return std::const_pointer_cast<ossia::scene_state>(state);

  auto out = std::make_shared<ossia::scene_state>(*state);

  if(state->roots && !state->roots->empty())
  {
    auto fresh_roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
    fresh_roots->reserve(state->roots->size());
    bool any_rewrite = false;
    for(const auto& root : *state->roots)
    {
      auto rewritten = rewriteNode(root, override);
      if(rewritten.get() != root.get())
        any_rewrite = true;
      fresh_roots->push_back(std::move(rewritten));
    }
    if(any_rewrite)
      out->roots = std::move(fresh_roots);
  }

  out->version = state->version + 1;
  out->dirty_index = state->dirty_index + 1;
  return out;
}

} // namespace Threedim::PrimitiveCloud
