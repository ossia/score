#pragma once
#include <Threedim/GeometryToBufferStrategies.hpp>

#include <halp/buffer.hpp>
#include <halp/controls.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>

namespace Threedim
{
// Name-based version of Threedim::ExtractBuffer.
//
// The original ExtractBuffer enumerates a fixed list of attribute slots
// (Position / TexCoord / Normal / ... / Buffer_0..Buffer_8) and selects
// one through a combobox. The Buffer_i path was wrong (the source buffer
// pointer was not refreshed in update(), and there was no way to refer
// to a buffer through anything other than its raw index in the mesh's
// buffer list, which is brittle whenever the upstream geometry rebuilds
// its buffer layout).
//
// This version takes:
//   * a Mode enum  -- Attribute or Buffer
//   * a name       -- a free-form string interpreted differently per mode
//
// Mode == Attribute: extract a single per-vertex attribute (one vec lane)
//   * "position" / "normal" / "tangent" / "bitangent" /
//     "texcoord" or "texcoord0".."texcoord7" / "uv" (alias for texcoord0) /
//     "color" or "color0".."color3" :
//        match against halp::attribute_semantic
//   * "<integer>" :  Nth entry in mesh.attributes[]
//   * anything else: custom-name lookup in mesh.attributes[].name
//   The output is one of the existing extraction strategies
//   (Direct / Compute / Indexed) just like ExtractBuffer.
//
// Mode == Buffer: extract a whole raw buffer (all bytes)
//   * "<integer>" :  the Nth entry in mesh.buffers[] (the index path
//     ExtractBuffer's combobox tried to expose)
//   * "index"     :  the buffer mesh.index points at
//   * a name matching one of `mesh.auxiliary[].name` (checked first,
//     since user-chosen aux names may shadow attribute names):
//     returns the auxiliary's backing buffer + its byte_offset /
//     byte_size. This is how ScenePreprocessor's per-frame auxiliaries
//     (camera, camera_prev, env, scene_lights, scene_materials,
//     per_draws, indirect_draw_cmds, scene_counts, and every
//     scene_data_ptr name) can be pulled out onto a standalone
//     gpu_buffer outlet for downstream consumers that don't want to
//     auto-bind via try_bind_from_geometry.
//   * anything else: look up an attribute by semantic / custom name
//     and return the buffer it lives in (via attribute -> input ->
//     buffer).
//
// On every update() the source buffer handle is re-fetched from the
// mesh, so an upstream that rebuilds its QRhiBuffer (resize / new
// allocation) is reflected on the next frame instead of leaving us
// holding a stale pointer.
class ExtractBuffer2
{
public:
  halp_meta(name, "Extract buffer (by name)")
  halp_meta(category, "Visuals/Utilities")
  halp_meta(c_name, "extract_buffer_by_name")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/extract-buffer.html")
  halp_meta(uuid, "3c9d6c2b-1f04-4f7d-9bc2-a4b1d7c8e5f0")

  enum Mode
  {
    Attribute,
    Buffer
  };

  struct ins
  {
    struct
    {
      halp_meta(name, "Geometry");
      halp::dynamic_gpu_geometry mesh;
      float transform[16]{};
      bool dirty_mesh = false;
      bool dirty_transform = false;
    } geometry;

    halp::combobox_t<"Mode", Mode> mode;
    struct : halp::lineedit<"Name / index", "position">
    {
      halp_meta(symbol, "name")
    } name;
    halp::toggle<"Pad vec3 to vec4"> pad_to_vec4;
  } inputs;

  struct
  {
    halp::gpu_buffer_output<"Buffer"> buffer;
  } outputs;

  ExtractBuffer2();

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res);
  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e);
  void release(score::gfx::RenderList& r);
  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
      QRhiResourceUpdateBatch*& res, score::gfx::Edge& edge);
  void operator()();

private:
  // Resolve the user's name string to an attribute_lookup, taking the
  // active mesh into account. Returns nullopt on miss.
  [[nodiscard]] static std::optional<attribute_lookup>
  resolveAttribute(const halp::dynamic_gpu_geometry& mesh, std::string_view n) noexcept;

  // Resolve the user's name string to a (buffer index, byte_offset, byte_size)
  // triple suitable for DirectBufferReferenceStrategy. Returns -1 on miss.
  struct BufferRef
  {
    int buffer_index{-1};
    int64_t byte_offset{};
    int64_t byte_size{};
  };
  [[nodiscard]] static BufferRef
  resolveBuffer(const halp::dynamic_gpu_geometry& mesh, std::string_view n) noexcept;

  // (Re)initialise m_strategy based on the current inputs and mesh.
  void initStrategy(score::gfx::RenderList& renderer);
  void updateOutput();

  ExtractionStrategyVariant m_strategy;
  Mode m_currentMode{Attribute};
  std::string m_currentName{};
  bool m_currentPadToVec4{false};
};
}
