#pragma once
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <ossia/dataflow/geometry_port.hpp>
#include <ossia/detail/hash_map.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace Threedim
{

// Injects runtime GPU textures and/or factor overrides into a scene's
// material table. The primary live-VJ use case: drop a video texture
// (or HDR shader output) onto an existing material without reloading
// the scene. Authored on top of the Dynamic Texture pathway in
// ScenePreprocessor — the texture handle is forwarded verbatim and
// ScenePreprocessor emits it as a `*Dyn<slot>` auxiliary-texture binding
// that classic_pbr_full (and any shader opting into the DYNAMIC source
// branch) samples directly.
//
// Scope: the four PBR slots (base color / metal-rough / normal /
// emissive). Occlusion and extension textures (transmission, clearcoat,
// sheen…) are not in the ScenePreprocessor's array pool yet, so
// overriding them here would have no effect downstream.
//
// Mode:
//   All      — every material in the scene gets the override applied.
//   ByIndex  — only `scene.state->materials[Index]` is overridden. Other
//              materials pass through unchanged. Use Scene Inspector +
//              the ByIndex variant to target a single object.
//
// Factor toggles gate whether the scalar/vector controls take effect;
// textures auto-gate on "handle is non-null" so an unwired inlet is a
// no-op regardless of state.
class MaterialOverride
{
public:
  halp_meta(name, "Material Override")
  halp_meta(category, "Visuals/3D/Scene")
  halp_meta(c_name, "material_override")
  halp_meta(authors, "ossia team")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/material-override.html")
  halp_meta(uuid, "c3d8e5f2-9a4b-4e7d-b8c1-2f6a9e3d5b7c")

  enum Mode
  {
    All,
    ByIndex
  };

  struct ins
  {
    struct
    {
      halp_meta(name, "Scene In");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene_in;

    // Port-driven rebuild: scalar controls trigger rebuild() via their
    // update() callbacks. Texture handles are checked in operator()()
    // because their native handles can change without a port-update
    // event (live video / CSF outputs swap native handles mid-stream).
    struct : halp::combobox_t<"Mode", Mode>
    {
      struct range
      {
        std::string_view values[2]{"All", "By Index"};
        int init{0};
      };
      void update(MaterialOverride& n) { n.rebuild(); }
    } mode;
    struct : halp::spinbox_i32<"Index", halp::irange{0, 4096, 0}>
    { void update(MaterialOverride& n) { n.rebuild(); } } index;

    // Texture overrides. Unwired (handle==nullptr) → pass through.
    // Handle changes detected in operator()(), not via control update().
    halp::gpu_texture_input<"Base Color Tex"> base_color_tex;
    halp::gpu_texture_input<"Metal Rough Tex"> metal_rough_tex;
    halp::gpu_texture_input<"Normal Tex"> normal_tex;
    halp::gpu_texture_input<"Emissive Tex"> emissive_tex;

    // Factor overrides. Gated on the companion toggles; otherwise the
    // original factor from the loader passes through.
    struct : halp::toggle<"Use base color">
    { void update(MaterialOverride& n) { n.rebuild(); } } use_base_color;
    struct : halp::hslider_f32<"R", halp::range{0., 1., 1.}>
    { void update(MaterialOverride& n) { n.rebuild(); } } base_r;
    struct : halp::hslider_f32<"G", halp::range{0., 1., 1.}>
    { void update(MaterialOverride& n) { n.rebuild(); } } base_g;
    struct : halp::hslider_f32<"B", halp::range{0., 1., 1.}>
    { void update(MaterialOverride& n) { n.rebuild(); } } base_b;
    struct : halp::hslider_f32<"A", halp::range{0., 1., 1.}>
    { void update(MaterialOverride& n) { n.rebuild(); } } base_a;

    struct : halp::toggle<"Use metallic">
    { void update(MaterialOverride& n) { n.rebuild(); } } use_metallic;
    struct : halp::hslider_f32<"Metallic", halp::range{0., 1., 0.}>
    { void update(MaterialOverride& n) { n.rebuild(); } } metallic;

    struct : halp::toggle<"Use roughness">
    { void update(MaterialOverride& n) { n.rebuild(); } } use_roughness;
    struct : halp::hslider_f32<"Roughness", halp::range{0., 1., 0.5}>
    { void update(MaterialOverride& n) { n.rebuild(); } } roughness;

    struct : halp::toggle<"Use emissive">
    { void update(MaterialOverride& n) { n.rebuild(); } } use_emissive;
    struct : halp::hslider_f32<"Emissive R", halp::range{0., 10., 0.}>
    { void update(MaterialOverride& n) { n.rebuild(); } } em_r;
    struct : halp::hslider_f32<"Emissive G", halp::range{0., 10., 0.}>
    { void update(MaterialOverride& n) { n.rebuild(); } } em_g;
    struct : halp::hslider_f32<"Emissive B", halp::range{0., 10., 0.}>
    { void update(MaterialOverride& n) { n.rebuild(); } } em_b;
    struct : halp::hslider_f32<"Emissive strength", halp::range{0., 10., 1.}>
    { void update(MaterialOverride& n) { n.rebuild(); } } em_strength;
  } inputs;

  struct outs
  {
    struct
    {
      halp_meta(name, "Scene Out");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene_out;
  } outputs;

  void rebuild();
  void operator()();

  // Cached output; stable shared_ptr identity when inputs haven't
  // changed so ScenePreprocessor's per-frame fingerprint fast-path stays
  // warm. Dynamic-texture swaps still propagate because ScenePreprocessor
  // refreshes its dynamic-slot map every frame (keyed on native_handle).
  std::shared_ptr<const ossia::scene_state> m_cached_out;
  uint8_t m_pending_dirty{0xFF};

  // Cache of override clones keyed by source material_component*. We
  // reuse the same std::shared_ptr<ossia::material_component> clone
  // across rebuilds when the source is unchanged, MUTATING its fields
  // in place. That keeps the shared_ptr address stable → the
  // preprocessor's m_loaderMaterialSlots keeps the arena slot allocated
  // across frames → no per-frame GC + re-allocate cycle → the Material
  // arena SSBO content is stable without churn. When the upstream
  // material list changes structurally, stale cache entries are
  // garbage-collected in rebuild().
  ossia::hash_map<
      const ossia::material_component*,
      std::shared_ptr<ossia::material_component>>
      m_clone_cache;

  // Identity cache: (input-scene pointer, input version, control values,
  // texture handles). If all match, we reuse m_cached_out without
  // rebuilding the materials list.
  const ossia::scene_state* m_cached_in_state{};
  int64_t m_cached_in_version{-1};
  int m_cached_mode{-1};
  int m_cached_index{-1};
  void* m_cached_tex[4]{};
  bool m_cached_use_base{false};
  bool m_cached_use_metallic{false};
  bool m_cached_use_roughness{false};
  bool m_cached_use_emissive{false};
  float m_cached_base[4]{};
  float m_cached_metallic{-1.f};
  float m_cached_roughness{-1.f};
  float m_cached_em[4]{};
  int64_t m_version_counter{0};
};

}
