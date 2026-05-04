#include "MaterialOverride.hpp"

#include <algorithm>

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
  // sampler stays default
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

} // namespace

void MaterialOverride::rebuild()
{
  const auto& in = inputs.scene_in.scene;
  const ossia::scene_state* in_state = in.state.get();
  const int64_t in_version = in_state ? in_state->version : -1;

  void* cur_tex[4]{
      inputs.base_color_tex.texture.handle,
      inputs.metal_rough_tex.texture.handle,
      inputs.normal_tex.texture.handle,
      inputs.emissive_tex.texture.handle};

  // No texture overrides and no factor overrides → passthrough. Keeps
  // downstream identity caches warm for the common "unconfigured" case.
  const bool any_tex = cur_tex[0] || cur_tex[1] || cur_tex[2] || cur_tex[3];
  const bool any_factor = inputs.use_base_color.value || inputs.use_metallic.value
                          || inputs.use_roughness.value
                          || inputs.use_emissive.value;
  if(!any_tex && !any_factor)
  {
    m_cached_out = in.state;
    m_pending_dirty = 0xFF;
    return;
  }

  const float cur_base[4]{
      inputs.base_r.value, inputs.base_g.value,
      inputs.base_b.value, inputs.base_a.value};
  const float cur_em[4]{
      inputs.em_r.value, inputs.em_g.value, inputs.em_b.value,
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
    // before — MUTATING its fields in place. The shared_ptr address
    // stays stable across rebuilds, so the preprocessor's
    // m_loaderMaterialSlots keeps the material arena slot allocated
    // across frames: no per-frame GC + reallocate churn, Material arena
    // content stays hot for SSBO-direct shader reads (task 28a).
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

  auto state = std::make_shared<ossia::scene_state>();
  // Passthrough: roots / cameras / animations / skeletons / environment
  // all reference the upstream shared_ptrs (no deep copy). Only materials
  // is swapped out.
  state->roots = in_state->roots;
  state->animations = in_state->animations;
  state->cameras = in_state->cameras;
  state->skeletons = in_state->skeletons;
  state->environment = in_state->environment;
  state->active_camera_id = in_state->active_camera_id;
  state->materials = std::move(new_mats);
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
      inputs.base_color_tex.texture.handle,
      inputs.metal_rough_tex.texture.handle,
      inputs.normal_tex.texture.handle,
      inputs.emissive_tex.texture.handle};
  const bool upstream_changed
      = m_cached_in_state != in_state || m_cached_in_version != in_version
        || m_cached_tex[0] != cur_tex[0] || m_cached_tex[1] != cur_tex[1]
        || m_cached_tex[2] != cur_tex[2] || m_cached_tex[3] != cur_tex[3];
  if(!m_cached_out || upstream_changed)
    rebuild();
  outputs.scene_out.scene.state = m_cached_out;
  outputs.scene_out.dirty = m_pending_dirty;
  m_pending_dirty = 0;
}

} // namespace Threedim
