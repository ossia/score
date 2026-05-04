#pragma once
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <cstdint>

namespace Threedim
{

// N-way scene_spec switch. Pick one of up to 4 scene inputs to pass
// through by index; live VJ-style A/B/C/D scene cutting.
//
// Unwired inputs are skipped — if `index` points at an empty slot, the
// node emits an empty scene, which downstream treats as "nothing to
// render" (no error). This makes it safe to leave slots open during
// authoring and fill them in incrementally.
//
// For blending between scenes: don't do it at the scene-graph level.
// Render each scene to its own texture (ScenePreprocessor → classic_pbr
// → BackgroundNode with a texture output) and ISF-crossfade the
// textures. Scene-level blending has no meaningful semantics for
// arbitrarily-different scene trees.
class SceneSwitch
{
public:
  halp_meta(name, "Scene Switch")
  halp_meta(category, "Visuals/3D/Scene")
  halp_meta(c_name, "scene_switch")
  halp_meta(authors, "ossia team")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/scene-switch.html")
  halp_meta(uuid, "7d5c3f8a-2e9b-4a1c-8f6d-5b3e0d9a7c4f")

  struct ins
  {
    struct
    {
      halp_meta(name, "Scene 0");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene0;
    struct
    {
      halp_meta(name, "Scene 1");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene1;
    struct
    {
      halp_meta(name, "Scene 2");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene2;
    struct
    {
      halp_meta(name, "Scene 3");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene3;

    halp::spinbox_i32<"Index", halp::irange{0, 3, 0}> index;
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

  void operator()()
  {
    const int idx = inputs.index.value;
    const ossia::scene_spec* picked = nullptr;
    switch(idx)
    {
      case 0: picked = &inputs.scene0.scene; break;
      case 1: picked = &inputs.scene1.scene; break;
      case 2: picked = &inputs.scene2.scene; break;
      case 3: picked = &inputs.scene3.scene; break;
      default: picked = &inputs.scene0.scene; break;
    }
    outputs.scene_out.scene = *picked;
    // Dirty flag drives downstream re-evaluation. Only raise it when
    // the selected input's contents are non-empty — otherwise
    // downstream shouldn't react to "we're on an empty slot".
    outputs.scene_out.dirty = picked->state ? 0xFF : 0;
  }
};

}
