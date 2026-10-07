#include "MaterialOverride.hpp"

#include <algorithm>
#include <optional>

namespace Threedim
{

namespace
{

// Copy a gpu texture handle from halp into an ossia texture_ref.
// We only populate the `texture` field — `source` stays null so the
// ScenePreprocessor's channelDynamicHandle() treats this ref as DYNAMIC.
// Sampler state is left at its default (linear/linear/repeat); can be
// exposed as controls later if needed.
void applyTextureOverride(
    ossia::texture_ref& dst, const halp::gpu_texture& src) noexcept
{
  dst.source.reset();
  dst.texture.native_handle = src.handle;
  dst.texture.bindless_index = 0;
}

// Decide whether a given material-index should receive overrides, given
// the mode and index inputs.
bool shouldOverride(int idx, int mode, int override_index) noexcept
{
  switch(mode)
  {
    case MaterialOverride::All:     return true;
    case MaterialOverride::ByIndex: return idx == override_index;
    default:                        return false;
  }
}

// Rewrites the scene tree so that every primitive (and standalone
// material payload) referencing a targeted material points at its clone.
// Subtrees without such a reference are returned as-is, by identity.
struct MaterialTreeRewriter
{
  using material_map = ossia::hash_map<
      const ossia::material_component*, ossia::material_component_ptr>;
  using mesh_cache = decltype(MaterialOverride::m_mesh_cache);

  const material_map& clones;
  mesh_cache& meshes;
  ossia::hash_set<const ossia::mesh_component*>& seen_meshes;
  ossia::hash_map<const ossia::scene_node*, ossia::scene_node_ptr> nodes{};

  ossia::material_component_ptr
  material(const ossia::material_component_ptr& m) const noexcept
  {
    if(!m)
      return m;
    auto it = clones.find(m.get());
    return it != clones.end() ? it->second : m;
  }

  // True when `dst` holds the clones of `src`'s materials, slot for slot.
  bool swapped(const ossia::mesh_component& src, const ossia::mesh_component& dst)
      const noexcept
  {
    if(src.primitives.size() != dst.primitives.size())
      return false;
    for(std::size_t i = 0; i < src.primitives.size(); ++i)
    {
      const auto& sp = src.primitives[i];
      const auto& dp = dst.primitives[i];
      if(material(sp.material) != dp.material
         || sp.material_variants.size() != dp.material_variants.size())
        return false;
      for(std::size_t v = 0; v < sp.material_variants.size(); ++v)
        if(material(sp.material_variants[v]) != dp.material_variants[v])
          return false;
    }
    return true;
  }

  ossia::mesh_component_ptr mesh(const ossia::mesh_component_ptr& src)
  {
    if(!src || swapped(*src, *src))
      return src;
    seen_meshes.insert(src.get());
    if(auto it = meshes.find(src.get()); it != meshes.end())
      if(swapped(*src, *it->second.second))
        return it->second.second;

    auto copy = std::make_shared<ossia::mesh_component>(*src);
    for(auto& prim : copy->primitives)
    {
      prim.material = material(prim.material);
      for(auto& v : prim.material_variants)
        v = material(v);
    }
    meshes[src.get()] = {src, copy};
    return copy;
  }

  ossia::scene_node_ptr node(const ossia::scene_node_ptr& src)
  {
    if(!src || !src->children)
      return src;
    if(auto it = nodes.find(src.get()); it != nodes.end())
      return it->second;

    std::shared_ptr<std::vector<ossia::scene_payload>> kids;
    const auto& children = *src->children;
    for(std::size_t i = 0; i < children.size(); ++i)
    {
      const auto& p = children[i];
      std::optional<ossia::scene_payload> replaced;
      if(auto* n = ossia::get_if<ossia::scene_node_ptr>(&p))
      {
        if(auto r = node(*n); r != *n)
          replaced = std::move(r);
      }
      else if(auto* m = ossia::get_if<ossia::mesh_component_ptr>(&p))
      {
        if(auto r = mesh(*m); r != *m)
          replaced = std::move(r);
      }
      else if(auto* ic = ossia::get_if<ossia::instance_component_ptr>(&p))
      {
        if(*ic)
        {
          if(auto r = mesh((*ic)->prototype); r != (*ic)->prototype)
          {
            auto inst = std::make_shared<ossia::instance_component>(**ic);
            inst->prototype = std::move(r);
            replaced = ossia::instance_component_ptr(std::move(inst));
          }
        }
      }
      else if(auto* mat = ossia::get_if<ossia::material_component_ptr>(&p))
      {
        if(auto r = material(*mat); r != *mat)
          replaced = std::move(r);
      }

      if(replaced)
      {
        if(!kids)
          kids = std::make_shared<std::vector<ossia::scene_payload>>(children);
        (*kids)[i] = std::move(*replaced);
      }
    }

    ossia::scene_node_ptr res = src;
    if(kids)
    {
      auto copy = std::make_shared<ossia::scene_node>(*src);
      copy->children = std::move(kids);
      res = std::move(copy);
    }
    nodes[src.get()] = res;
    return res;
  }
};

} // namespace

void MaterialOverride::rebuild()
{
  const auto& in = inputs.scene_in.scene;
  const ossia::scene_state* in_state = in.state.get();
  const int64_t in_version = in_state ? in_state->version : -1;

  void* cur_tex[4]{
      texture2DHandle(inputs.base_color_tex.texture),
      texture2DHandle(inputs.metal_rough_tex.texture),
      texture2DHandle(inputs.normal_tex.texture),
      texture2DHandle(inputs.emissive_tex.texture)};

  // No texture overrides and no factor overrides → passthrough. Keeps
  // downstream identity caches warm for the common "unconfigured" case.
  const bool any_tex = cur_tex[0] || cur_tex[1] || cur_tex[2] || cur_tex[3];
  const bool any_factor = inputs.use_base_color.value || inputs.use_metallic.value
                          || inputs.use_roughness.value
                          || inputs.use_emissive.value;
  if(!any_tex && !any_factor)
  {
    // Cache the input identity here too: without it operator()() sees a
    // stale m_cached_in_state on every tick and re-runs rebuild(), which
    // re-emits dirty = 0xFF forever in the node's default state (the TagAs
    // defect class).
    m_cached_in_state = in_state;
    m_cached_in_version = in_version;
    std::copy(cur_tex, cur_tex + 4, m_cached_tex);
    m_cached_out = in.state;
    m_pending_dirty = 0xFF;
    m_mesh_cache.clear();
    return;
  }

  const float cur_base[4]{
      inputs.base_color.value.r, inputs.base_color.value.g,
      inputs.base_color.value.b, inputs.base_color.value.a};
  const float cur_em[4]{
      inputs.emissive.value.r, inputs.emissive.value.g, inputs.emissive.value.b,
      inputs.em_strength.value};

  m_cached_in_state = in_state;
  m_cached_in_version = in_version;
  m_cached_mode = inputs.mode.value;
  m_cached_index = inputs.index.value;
  std::copy(cur_tex, cur_tex + 4, m_cached_tex);
  m_cached_use_base = inputs.use_base_color.value;
  m_cached_use_metallic = inputs.use_metallic.value;
  m_cached_use_roughness = inputs.use_roughness.value;
  m_cached_use_emissive = inputs.use_emissive.value;
  std::copy(cur_base, cur_base + 4, m_cached_base);
  m_cached_metallic = inputs.metallic.value;
  m_cached_roughness = inputs.roughness.value;
  std::copy(cur_em, cur_em + 4, m_cached_em);

  if(!in_state || !in_state->materials || in_state->materials->empty())
  {
    m_cached_out = in.state;
    m_pending_dirty = 0xFF;
    return;
  }

  const auto& src_mats = *in_state->materials;
  auto new_mats = std::make_shared<std::vector<ossia::material_component_ptr>>();
  new_mats->reserve(src_mats.size());

  // Track which source materials we clone this cycle so we can GC stale
  // entries from m_clone_cache (freed when upstream shrinks or swaps).
  ossia::hash_set<const ossia::material_component*> seen_src;
  seen_src.reserve(src_mats.size());
  MaterialTreeRewriter::material_map clones;

  for(std::size_t i = 0; i < src_mats.size(); ++i)
  {
    const auto& src_mat = src_mats[i];
    if(!src_mat || !shouldOverride((int)i, inputs.mode.value, inputs.index.value))
    {
      new_mats->push_back(src_mat);
      continue;
    }
    seen_src.insert(src_mat.get());

    // Reuse the cached clone shared_ptr if we've cloned this source
    // before, mutating its fields in place. The shared_ptr address
    // stays stable across rebuilds, so the preprocessor's
    // m_loaderMaterialSlots keeps the material arena slot allocated
    // across frames: no per-frame GC + reallocate churn, Material arena
    // content stays hot for SSBO-direct shader reads.
    // stable_id is inherited from the source via the copy — the
    // fingerprint sees the override as the same logical material.
    auto it = m_clone_cache.find(src_mat.get());
    std::shared_ptr<ossia::material_component> cloned;
    if(it != m_clone_cache.end())
    {
      // Reuse: start from the original upstream fields every rebuild to
      // avoid accumulating stale override state (e.g. when the user
      // toggles 'use_metallic' off, the factor must revert to
      // upstream's).
      cloned = it->second;
      *cloned = *src_mat;
    }
    else
    {
      cloned = std::make_shared<ossia::material_component>(*src_mat);
      m_clone_cache.emplace(src_mat.get(), cloned);
    }

    if(cur_tex[0])
      applyTextureOverride(cloned->base_color_texture, inputs.base_color_tex.texture);
    if(cur_tex[1])
      applyTextureOverride(
          cloned->metallic_roughness_texture, inputs.metal_rough_tex.texture);
    if(cur_tex[2])
      applyTextureOverride(cloned->normal_texture, inputs.normal_tex.texture);
    if(cur_tex[3])
      applyTextureOverride(cloned->emissive_texture, inputs.emissive_tex.texture);

    if(inputs.use_base_color.value)
    {
      cloned->base_color_factor[0] = cur_base[0];
      cloned->base_color_factor[1] = cur_base[1];
      cloned->base_color_factor[2] = cur_base[2];
      cloned->base_color_factor[3] = cur_base[3];
    }
    if(inputs.use_metallic.value)
      cloned->metallic_factor = inputs.metallic.value;
    if(inputs.use_roughness.value)
      cloned->roughness_factor = inputs.roughness.value;
    if(inputs.use_emissive.value)
    {
      cloned->emissive_factor[0] = cur_em[0];
      cloned->emissive_factor[1] = cur_em[1];
      cloned->emissive_factor[2] = cur_em[2];
      cloned->emissive_strength = cur_em[3];
    }

    clones[src_mat.get()] = cloned;
    new_mats->push_back(cloned);
  }

  // GC cache entries whose source material vanished from upstream.
  for(auto it = m_clone_cache.begin(); it != m_clone_cache.end();)
  {
    if(seen_src.find(it->first) == seen_src.end())
      it = m_clone_cache.erase(it);
    else
      ++it;
  }

  // Forward every shared scene_state field (roots / cameras / animations /
  // skeletons / environment / collections / variants / time / statistics /
  // aux injections) by copying the upstream state wholesale — the vectors
  // are shared_ptrs so this is shallow — then swap the materials, and the
  // roots when a mesh in the tree uses a targeted material.
  // Cherry-picking fields here silently loses data on every pass, which
  // tests/threedim/Transform3DCompose.cpp pins for wrapSceneWithTransform.
  auto state = std::make_shared<ossia::scene_state>(*in_state);
  state->materials = std::move(new_mats);

  ossia::hash_set<const ossia::mesh_component*> seen_meshes;
  if(in_state->roots)
  {
    MaterialTreeRewriter rw{clones, m_mesh_cache, seen_meshes};
    std::shared_ptr<std::vector<ossia::scene_node_ptr>> roots;
    const auto& src_roots = *in_state->roots;
    for(std::size_t i = 0; i < src_roots.size(); ++i)
    {
      auto r = rw.node(src_roots[i]);
      if(r == src_roots[i])
        continue;
      if(!roots)
        roots = std::make_shared<std::vector<ossia::scene_node_ptr>>(src_roots);
      (*roots)[i] = std::move(r);
    }
    if(roots)
      state->roots = std::move(roots);
  }
  for(auto it = m_mesh_cache.begin(); it != m_mesh_cache.end();)
  {
    if(seen_meshes.find(it->first) == seen_meshes.end())
      it = m_mesh_cache.erase(it);
    else
      ++it;
  }
  state->version = ++m_version_counter;
  state->dirty_index = m_version_counter;

  m_cached_out = state;
  m_pending_dirty = 0xFF;
}

void MaterialOverride::operator()()
{
  // Upstream scene_state and live texture handles can change without a
  // port-update event (upstream runs per-tick; video/CSF textures swap
  // native handles mid-stream). Detect those here and trigger rebuild.
  const auto& in = inputs.scene_in.scene;
  const ossia::scene_state* in_state = in.state.get();
  const int64_t in_version = in_state ? in_state->version : -1;
  void* cur_tex[4]{
      texture2DHandle(inputs.base_color_tex.texture),
      texture2DHandle(inputs.metal_rough_tex.texture),
      texture2DHandle(inputs.normal_tex.texture),
      texture2DHandle(inputs.emissive_tex.texture)};
  const bool upstream_changed
      = m_cached_in_state != in_state || m_cached_in_version != in_version
        || m_cached_tex[0] != cur_tex[0] || m_cached_tex[1] != cur_tex[1]
        || m_cached_tex[2] != cur_tex[2] || m_cached_tex[3] != cur_tex[3];
  if(upstream_changed)
    rebuild();
  outputs.scene_out.scene.state = m_cached_out;
  outputs.scene_out.dirty = m_pending_dirty;
  m_pending_dirty = 0;
}

} // namespace Threedim
