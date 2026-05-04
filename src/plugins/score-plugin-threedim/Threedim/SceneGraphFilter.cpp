#include "SceneGraphFilter.hpp"

#include <ossia/network/value/value.hpp>

#include <algorithm>

namespace Threedim
{

namespace
{

// ───── Glob matching ─────────────────────────────────────────────────
// Minimal glob: `*` matches anything except `/`, `**` matches across
// slashes, `?` matches a single non-slash character, everything else
// is literal. Good enough for path-style filters; `std::regex` is the
// fallback if users want full regex later.
bool glob_match(std::string_view pattern, std::string_view text) noexcept
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
        // Detect `**` for slash-crossing wildcard.
        star_double = (pi + 1 < pattern.size() && pattern[pi + 1] == '*');
        if(star_double)
          pi += 2;
        else
          pi += 1;
        star_pi = pi;
        star_ti = ti;
        continue;
      }
      if(pc == '?')
      {
        if(text[ti] == '/')
        {
          // `?` can't cross slashes; bail to backtrack below.
        }
        else
        {
          ++pi;
          ++ti;
          continue;
        }
      }
      else if(pc == text[ti])
      {
        ++pi;
        ++ti;
        continue;
      }
    }
    // Mismatch — backtrack to last star.
    if(star_pi != std::string_view::npos)
    {
      // `*` can't eat a slash; `**` can.
      if(!star_double && text[star_ti] == '/')
        return false;
      pi = star_pi;
      ++star_ti;
      ti = star_ti;
      continue;
    }
    return false;
  }
  // Consume trailing stars.
  while(pi < pattern.size() && pattern[pi] == '*')
    ++pi;
  return pi == pattern.size();
}

// Return true if any pattern in `patterns` matches `text`.
bool any_match(
    const std::vector<std::string>& patterns, std::string_view text) noexcept
{
  for(const auto& pat : patterns)
    if(glob_match(pat, text))
      return true;
  return false;
}

// ───── Predicate context ─────────────────────────────────────────────

struct FilterCtx
{
  SceneGraphFilter::Mode mode;
  bool invert;
  SceneGraphFilter::Component component;
  const std::vector<std::string>& paths;
  const std::vector<std::string>& names;
  const std::vector<std::string>& material_tags;
  const ossia::scene_state* state;
  // Tier-1 extensions: schema-field + property predicates.
  SceneGraphFilter::AlphaMode alpha_mode;
  SceneGraphFilter::Purpose purpose;
  bool caster_flag;
  std::string_view prop_key;
  SceneGraphFilter::PropertyOp prop_op;
  std::string_view prop_value;
};

// True if the payload carried by a scene_node has the component kind
// we're looking for. Used by ByComponent mode.
bool node_has_component(
    const ossia::scene_node& n, SceneGraphFilter::Component which) noexcept
{
  if(!n.has_children())
    return false;
  for(const auto& p : *n.children)
  {
    switch(which)
    {
      case SceneGraphFilter::Mesh:
        if(ossia::get_if<ossia::mesh_component_ptr>(&p))
          return true;
        break;
      case SceneGraphFilter::Light:
        if(ossia::get_if<ossia::light_component_ptr>(&p))
          return true;
        break;
      case SceneGraphFilter::Camera:
        if(ossia::get_if<ossia::camera_component_ptr>(&p))
          return true;
        break;
      case SceneGraphFilter::Instance:
        if(ossia::get_if<ossia::instance_component_ptr>(&p))
          return true;
        break;
      case SceneGraphFilter::Skeleton:
        if(ossia::get_if<ossia::skeleton_component_ptr>(&p))
          return true;
        break;
    }
  }
  return false;
}

// Does this node match the current mode's predicate before `invert` is
// applied? `path` is the slash-joined name chain from the root.
bool node_matches(
    const ossia::scene_node& n, std::string_view path,
    const FilterCtx& ctx) noexcept
{
  switch(ctx.mode)
  {
    case SceneGraphFilter::PassThrough:
      return true;
    case SceneGraphFilter::VisibleOnly:
      return n.visible;
    case SceneGraphFilter::ByPath:
      return any_match(ctx.paths, path);
    case SceneGraphFilter::ByName:
      return any_match(ctx.names, n.name);
    case SceneGraphFilter::ByComponent:
      return node_has_component(n, ctx.component);
    case SceneGraphFilter::ByMaterialTag: {
      // Check every mesh_component primitive's material tag against
      // the pattern list. mesh_primitive holds a direct
      // material_component_ptr — no index lookup into scene_state.materials.
      if(!n.has_children())
        return false;
      for(const auto& p : *n.children)
      {
        const auto* mesh = ossia::get_if<ossia::mesh_component_ptr>(&p);
        if(!mesh || !*mesh)
          continue;
        for(const auto& prim : (*mesh)->primitives)
        {
          if(prim.material
             && any_match(ctx.material_tags, prim.material->tag))
            return true;
        }
      }
      return false;
    }

    case SceneGraphFilter::SetVisibility:
      // SetVisibility uses the same predicate chain as ByName in the
      // caller — this case is a hint to the walker, not a true filter.
      // Fall through to "match everything" so the flag flip runs on
      // every node. The real gating happens at the caller level using
      // name-list matching.
      return true;

    // ─── Schema-field predicates (Tier 1 extension) ─────────────────
    case SceneGraphFilter::ByAlphaMode: {
      // Match when any primitive under this node has a material with
      // the selected alphaMode. Per-primitive check because one
      // scene_node can hold a mesh with multiple primitives using
      // different alpha modes.
      if(!n.has_children())
        return false;
      const auto want = static_cast<ossia::alpha_mode>(ctx.alpha_mode);
      for(const auto& p : *n.children)
      {
        const auto* mesh = ossia::get_if<ossia::mesh_component_ptr>(&p);
        if(!mesh || !*mesh)
          continue;
        for(const auto& prim : (*mesh)->primitives)
        {
          if(prim.material && prim.material->alpha == want)
            return true;
        }
      }
      return false;
    }

    case SceneGraphFilter::ByShadowCaster:
    case SceneGraphFilter::ByReflectionCaster: {
      // Read the selected bool flag from any of this node's materials.
      // Matches when any primitive's material has the flag == caster_flag.
      if(!n.has_children())
        return false;
      for(const auto& p : *n.children)
      {
        const auto* mesh = ossia::get_if<ossia::mesh_component_ptr>(&p);
        if(!mesh || !*mesh)
          continue;
        for(const auto& prim : (*mesh)->primitives)
        {
          if(!prim.material)
            continue;
          const bool flag
              = (ctx.mode == SceneGraphFilter::ByShadowCaster)
                  ? prim.material->shadow_caster
                  : prim.material->reflection_caster;
          if(flag == ctx.caster_flag)
            return true;
        }
      }
      return false;
    }

    case SceneGraphFilter::ByPurpose:
      return static_cast<uint8_t>(n.purpose)
             == static_cast<uint8_t>(ctx.purpose);

    case SceneGraphFilter::ByNodeProperty:
    case SceneGraphFilter::ByMaterialProperty: {
      if(ctx.prop_key.empty())
        return false;
      auto match_prop
          = [&](const ossia::scene_property_map& props) -> bool {
        auto it = props.find(std::string(ctx.prop_key));
        if(it == props.end())
          return false;
        // Stringify the stored value for comparison. ossia::value is
        // variant-typed; value_to_pretty_string covers int/float/
        // string/bool/impulse uniformly.
        const std::string lhs = ossia::value_to_pretty_string(it->second);
        const std::string_view rhs = ctx.prop_value;
        switch(ctx.prop_op)
        {
          case SceneGraphFilter::PropEqual:       return lhs == rhs;
          case SceneGraphFilter::PropNotEqual:    return lhs != rhs;
          case SceneGraphFilter::PropContains:    return lhs.find(rhs) != std::string::npos;
          case SceneGraphFilter::PropLessThan:
          case SceneGraphFilter::PropGreaterThan: {
            // Numeric compare when both sides parse as float; fall
            // back to lexicographic compare otherwise. Covers the
            // common "alpha_cutoff > 0.5" case without a full DSL.
            try
            {
              const double l = std::stod(lhs);
              const double r = std::stod(std::string(rhs));
              return ctx.prop_op == SceneGraphFilter::PropLessThan
                         ? l < r : l > r;
            }
            catch(...)
            {
              return ctx.prop_op == SceneGraphFilter::PropLessThan
                         ? lhs < rhs : lhs > rhs;
            }
          }
        }
        return false;
      };

      if(ctx.mode == SceneGraphFilter::ByNodeProperty)
        return match_prop(n.properties);

      // ByMaterialProperty — check every primitive's material.
      if(!n.has_children())
        return false;
      for(const auto& p : *n.children)
      {
        const auto* mesh = ossia::get_if<ossia::mesh_component_ptr>(&p);
        if(!mesh || !*mesh)
          continue;
        for(const auto& prim : (*mesh)->primitives)
        {
          if(prim.material && match_prop(prim.material->properties))
            return true;
        }
      }
      return false;
    }
  }
  return true;
}

// ───── Tree walker ───────────────────────────────────────────────────
// Recursively copy the subtree, dropping nodes whose (possibly
// inverted) predicate says no. Subtrees with no match anywhere are
// returned as the original shared_ptr (structural sharing).

struct Walker
{
  const FilterCtx& ctx;

  // Does `node` or any descendant match? Memoization would help here
  // if the tree gets big; for now linear scan on each parent. glTF
  // scenes are typically shallow enough that this is fine.
  bool subtree_has_match(
      const ossia::scene_node& n, std::string path) const noexcept
  {
    if(node_matches(n, path, ctx))
      return true;
    if(!n.has_children())
      return false;
    for(const auto& p : *n.children)
    {
      if(auto* sub = ossia::get_if<ossia::scene_node_ptr>(&p))
      {
        if(!*sub)
          continue;
        std::string childPath
            = path + '/' + (*sub)->name;
        if(subtree_has_match(**sub, std::move(childPath)))
          return true;
      }
    }
    return false;
  }

  // Returns the rewritten node, or nullptr if this node (and its
  // entire subtree) should be dropped.
  ossia::scene_node_ptr rewrite(
      const ossia::scene_node_ptr& src, const std::string& path) const
  {
    if(!src)
      return nullptr;

    const bool self_matches = node_matches(*src, path, ctx);

    // SetVisibility mode: don't drop anything, just toggle `visible`
    // on matches. `invert` flips the sense: Invert=false → matches
    // become hidden; Invert=true → matches become visible.
    if(ctx.mode == SceneGraphFilter::SetVisibility)
    {
      const bool target_visible = ctx.invert;
      const bool need_change
          = self_matches && (src->visible != target_visible);

      // Recurse so descendants can also toggle.
      ossia::scene_node_ptr recursed_self = src;
      if(src->has_children())
      {
        auto new_children
            = std::make_shared<std::vector<ossia::scene_payload>>();
        new_children->reserve(src->children->size());
        bool child_changed = false;
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
              child_changed = true;
            new_children->push_back(rw ? rw : *sub);
          }
          else
          {
            new_children->push_back(payload);
          }
        }
        if(child_changed)
        {
          auto copy = std::make_shared<ossia::scene_node>(*src);
          copy->children = std::move(new_children);
          copy->dirty_index = src->dirty_index + 1;
          recursed_self = copy;
        }
      }

      if(need_change)
      {
        auto copy = std::make_shared<ossia::scene_node>(*recursed_self);
        copy->visible = target_visible;
        copy->dirty_index = recursed_self->dirty_index + 1;
        return copy;
      }
      return recursed_self;
    }

    const bool keep_self = ctx.invert ? !self_matches : self_matches;

    // In modes other than PassThrough: if this node doesn't match AND
    // no descendant does, drop the whole subtree.
    if(ctx.mode != SceneGraphFilter::PassThrough && !keep_self
       && !subtree_has_match(*src, path))
      return nullptr;

    // If no filtering is active (mode 0) and we reach here, share.
    if(ctx.mode == SceneGraphFilter::PassThrough)
      return src;

    // Recurse into children, rebuilding the payload list.
    if(!src->has_children())
      return keep_self ? src : nullptr;

    auto new_children
        = std::make_shared<std::vector<ossia::scene_payload>>();
    new_children->reserve(src->children->size());
    bool any_dropped = false;
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
        if(rw)
          new_children->push_back(rw);
        else
          any_dropped = true;
      }
      else
      {
        // Non-scene_node payloads (meshes, lights, transforms, etc.)
        // follow the node they're on: keep iff the node was kept.
        if(keep_self)
          new_children->push_back(payload);
        else
          any_dropped = true;
      }
    }

    if(!keep_self && new_children->empty())
      return nullptr; // nothing survived; drop the node wrapper too

    // Share-if-unchanged: when no child was rewritten AND no child
    // was dropped AND the node itself is kept, just return the
    // original pointer.
    if(!any_dropped && new_children->size() == src->children->size())
    {
      bool identical = true;
      for(std::size_t i = 0; i < new_children->size(); ++i)
      {
        if(auto* a = ossia::get_if<ossia::scene_node_ptr>(&(*new_children)[i]))
        {
          auto* b = ossia::get_if<ossia::scene_node_ptr>(
              &(*src->children)[i]);
          if(!b || a->get() != b->get())
          {
            identical = false;
            break;
          }
        }
      }
      if(identical)
        return src;
    }

    auto copy = std::make_shared<ossia::scene_node>(*src);
    copy->children = std::move(new_children);
    copy->dirty_index = src->dirty_index + 1;
    return copy;
  }
};

} // namespace

void SceneGraphFilter::rebuild()
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

  // PassThrough is the free path.
  if(inputs.mode.value == PassThrough)
  {
    m_cached_out = in.state;
    m_cached_in_state = in_state;
    m_cached_in_version = in_version;
    m_cached_mode = inputs.mode.value;
    m_cached_invert = inputs.invert.value;
    m_cached_component = inputs.component.value;
    m_cached_paths = inputs.paths.value;
    m_cached_names = inputs.names.value;
    m_cached_material_tags = inputs.material_tags.value;
    m_pending_dirty = 0xFF;
    return;
  }

  FilterCtx ctx{
      .mode = Mode(inputs.mode.value),
      .invert = inputs.invert.value,
      .component = Component(inputs.component.value),
      .paths = inputs.paths.value,
      .names = inputs.names.value,
      .material_tags = inputs.material_tags.value,
      .state = in.state.get(),
      .alpha_mode = AlphaMode(inputs.alpha_mode.value),
      .purpose = Purpose(inputs.purpose.value),
      .caster_flag = inputs.caster_flag.value,
      .prop_key = inputs.prop_key.value,
      .prop_op = PropertyOp(inputs.prop_op.value),
      .prop_value = inputs.prop_value.value};

  Walker w{ctx};
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

  m_cached_out = std::move(new_state);
  m_cached_in_state = in_state;
  m_cached_in_version = in_version;
  m_cached_mode = inputs.mode.value;
  m_cached_invert = inputs.invert.value;
  m_cached_component = inputs.component.value;
  m_cached_paths = inputs.paths.value;
  m_cached_names = inputs.names.value;
  m_cached_material_tags = inputs.material_tags.value;
  m_pending_dirty = 0xFF;
}

void SceneGraphFilter::operator()()
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
