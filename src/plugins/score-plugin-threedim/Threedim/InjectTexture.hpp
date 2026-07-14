#pragma once
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace Threedim
{

// Mid-pipeline aux-texture injection. Takes a scene_spec passthrough
// cable plus a live GPU texture from an upstream producer (video node,
// ISF output, CSF image, etc.) and attaches it under a caller-supplied
// name. ScenePreprocessor consumes `scene_state::inject_textures` and
// writes matching `auxiliary_texture` entries onto its output
// geometry — so the live handle flows to any downstream consumer
// shader that declares an AUXILIARY texture entry with the same name.
//
// Texture handles are routed via halp::gpu_texture_input, which goes
// through the Graph's TextureInlet / updateInputTexture() path — a
// fundamentally different mechanism from InjectBuffer's
// halp::gpu_buffer_input (which goes through bufferForInput / Output).
// Hence the split into two distinct node types.
//
// Wiring:
//   VideoProducer → InjectTexture(name="base_color_dyn0")
//                 → ScenePreprocessor → classic_pbr_full
class InjectTexture
{
public:
  halp_meta(name, "Inject Texture")
  halp_meta(category, "Visuals/3D/Scene")
  halp_meta(c_name, "inject_texture")
  halp_meta(authors, "ossia team")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/inject-texture.html")
  halp_meta(uuid, "3b8d2f7c-9a5e-4f1d-a4c6-6e2d9c4f8a1b")

  struct ins
  {
    struct
    {
      halp_meta(name, "Scene In");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene_in;

    // Port-driven rebuild: aux_name triggers rebuild(). scene_in +
    // texture handle changes detected in operator()() (no port-update
    // event fires when a native handle is swapped).
    // Live GPU texture from an upstream producer. Null handle → the
    // injection is skipped (passthrough).
    halp::gpu_texture_input<"Texture"> texture;

    struct : halp::lineedit<"Aux name", "">
    { void update(InjectTexture& n) { n.rebuild(); } } aux_name;
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

  std::shared_ptr<const ossia::scene_state> m_cached_out;
  uint8_t m_pending_dirty{0xFF};
  const ossia::scene_state* m_cached_in_state{};
  int64_t m_cached_in_version{-1};
  std::string m_cached_name;
  void* m_cached_handle{};
  int64_t m_version_counter{0};
};

}
