#pragma once
#include <halp/buffer.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <Gfx/Graph/GpuResourceRegistry.hpp>

#include <cstdint>

class QRhiResourceUpdateBatch;

namespace score::gfx
{
class RenderList;
struct Edge;
}

namespace Threedim
{

// Scene-level buffer extractor. Takes a scene_spec in, picks one of the
// GPU arena slots stamped on the scene's components, and republishes
// the backing `{QRhiBuffer*, byte_offset, byte_size}` triple on a
// halp::gpu_buffer outlet.
//
// Unlike Threedim::ExtractBuffer2 — which extracts from a flattened
// geometry's aux list downstream of a ScenePreprocessor — this node
// works directly on a raw scene_spec. Useful when:
//
//   - A custom compute shader wants to consume a producer's Raw arena
//     slot without paying the cost of a preprocessor flatten.
//   - The downstream pipeline has no preprocessor (e.g. a pure
//     data-probing tool inspecting the environment's bytes).
//
// Source resolution uses the `raw_slot` field on each component type:
//
//   - Environment: scene.state->environment.raw_slot
//   - Camera(N):   (*scene.state->cameras)[N]->raw_slot
//   - Material(N): (*scene.state->materials)[N]->raw_slot
//
// The registry's isLive() check guards every read. Stale refs (producer
// released, mismatched generation) clear the outlet rather than handing
// a dangling QRhiBuffer* downstream.
//
// Lights aren't exposed here because the light tree isn't a flat
// scene_state.lights vector (lights live as scene_payload children).
// Walking the tree to find the Nth light by preorder index is a
// reasonable future addition if the use case shows up; for now,
// extract light data downstream of a ScenePreprocessor via
// ExtractBuffer2(name="scene_lights").
class ExtractSceneBuffer
{
public:
  halp_meta(name, "Extract Scene Buffer")
  halp_meta(category, "Visuals/3D/Scene")
  halp_meta(c_name, "extract_scene_buffer")
  halp_meta(authors, "ossia team")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/extract-scene-buffer.html")
  halp_meta(uuid, "5f2b8e1c-4a7d-4e9b-b0f1-3c6e8d2a5b74")

  enum Kind
  {
    Environment,
    Camera,
    Material
  };

  struct ins
  {
    struct
    {
      halp_meta(name, "Scene In");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene_in;

    struct : halp::combobox_t<"Kind", Kind>
    {
      struct range
      {
        std::string_view values[3]{"Environment", "Camera", "Material"};
        int init{0};
      };
    } kind;

    // Index inside scene.state->cameras / ->materials. Ignored when
    // Kind == Environment (the environment is a singleton on scene_state).
    halp::spinbox_i32<"Index", halp::irange{0, 1024, 0}> index;
  } inputs;

  struct outs
  {
    halp::gpu_buffer_output<"Buffer"> buffer;
  } outputs;

  // Execution-thread tick. No heavy work here — just snapshot the
  // current scene ref + control values. Slot resolution needs the
  // registry (render thread) so it happens in update().
  void operator()();

  // Render-thread hooks. update() resolves the slot ref against the
  // renderer's GpuResourceRegistry, validates via isLive(), and
  // publishes the buffer handle + offset + size on the outlet. init()
  // and release() are no-ops for now — the node owns no GPU state.
  void init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res);
  void update(
      score::gfx::RenderList& r, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e);
  void release(score::gfx::RenderList& r);
};

}
