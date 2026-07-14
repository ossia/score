#pragma once
#include <halp/buffer.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace Threedim
{

// Mid-pipeline aux-buffer injection. Takes a scene_spec passthrough cable
// plus a live GPU buffer from an upstream producer (CSF output, another
// aux node, etc.) and attaches it to the scene as a pending injection
// under a caller-supplied name. ScenePreprocessor consumes
// `scene_state::inject_buffers` at flatten-time and writes matching
// `auxiliary_buffer` entries onto every output geometry — so the live
// handle ends up bound to any downstream consumer shader that declares
// an AUXILIARY entry with the same name (SSBO or UBO kind).
//
// Wiring:
//   CSFProducer → InjectBuffer(name="scene_params", is_uniform=true)
//                → ScenePreprocessor → classic_pbr_full
//
// Name collisions with existing auxes published by the scene producers
// (e.g., ScenePreprocessor's own scene_lights / scene_materials) follow
// last-wins — the injection appended after flatten overrides the
// flatten-time entry. Use this to selectively replace standard auxes
// with custom data without forking the preprocessor.
class InjectBuffer
{
public:
  halp_meta(name, "Inject Buffer")
  halp_meta(category, "Visuals/3D/Scene")
  halp_meta(c_name, "inject_buffer")
  halp_meta(authors, "ossia team")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/inject-buffer.html")
  halp_meta(uuid, "4f9a6e2d-7c83-4b5d-9e1f-8a3c5d6b2f4e")

  struct ins
  {
    struct
    {
      halp_meta(name, "Scene In");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene_in;

    // Port-driven rebuild: aux_name triggers rebuild(). scene_in +
    // buffer handle changes are detected in operator()() because they
    // can change without a port-update event.
    // Live GPU buffer from an upstream producer. Null handle → the
    // injection is skipped (passthrough), so unwiring is safe.
    halp::gpu_buffer_input<"Buffer"> buffer;

    struct : halp::lineedit<"Aux name", "">
    { void update(InjectBuffer& n) { n.rebuild(); } } aux_name;
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

  // Stable shared_ptr cached while inputs are unchanged — keeps
  // ScenePreprocessor's fingerprint fast-path warm.
  std::shared_ptr<const ossia::scene_state> m_cached_out;
  uint8_t m_pending_dirty{0xFF};
  const ossia::scene_state* m_cached_in_state{};
  int64_t m_cached_in_version{-1};
  std::string m_cached_name;
  void* m_cached_handle{};
  int64_t m_cached_byte_size{};
  int64_t m_version_counter{0};
};

}
