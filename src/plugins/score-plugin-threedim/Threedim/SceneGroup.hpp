#pragma once
#include <Threedim/TinyObj.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <Gfx/Graph/GpuResourceRegistry.hpp>

#include <QQuaternion>

#include <cstdint>
#include <memory>
#include <string>

class QRhiResourceUpdateBatch;

namespace score::gfx
{
class RenderList;
struct Edge;
}

namespace Threedim
{

// Wraps up to 4 scene inputs under a single named parent scene_node
// with its own TRS. The group name becomes addressable by downstream
// filters / overrides via the usual path pattern — so
// `SceneGroup(name="ProsceniumSet")` +
// `SceneGraphFilter(paths=["/ProsceniumSet/**"])` is the canonical
// "bundle and tag a subset" pattern.
//
// Materials / animations / cameras / skeletons / environment are
// merged additively from all inputs (first-wins on singletons like
// active_camera_id and environment), same convention as
// MergeGeometries / merge_scenes.
class SceneGroup
{
public:
  halp_meta(name, "Scene Group")
  halp_meta(category, "Visuals/3D/Scene")
  halp_meta(c_name, "scene_group")
  halp_meta(authors, "ossia team")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/scene-group.html")
  halp_meta(uuid, "8a3b5e2d-7c4f-4b9e-9d1a-6f8e2c5d3a7b")

  struct ins
  {
    struct { halp_meta(name, "Scene 0"); ossia::scene_spec scene; uint8_t dirty{0}; } scene0;
    struct { halp_meta(name, "Scene 1"); ossia::scene_spec scene; uint8_t dirty{0}; } scene1;
    struct { halp_meta(name, "Scene 2"); ossia::scene_spec scene; uint8_t dirty{0}; } scene2;
    struct { halp_meta(name, "Scene 3"); ossia::scene_spec scene; uint8_t dirty{0}; } scene3;

    // Port-driven rebuild: controls trigger rebuild(); upstream scene
    // inputs detected in operator()().
    struct : halp::lineedit<"Name", "">
    { void update(SceneGroup& n) { n.rebuild(); } } name;
    struct : PositionControl
    { void update(SceneGroup& n) { n.rebuild(); } } position;
    struct : RotationControl
    { void update(SceneGroup& n) { n.rebuild(); } } rotation;
    struct : ScaleControl
    { void update(SceneGroup& n) { n.rebuild(); } } scale;
  } inputs;

  struct outs
  {
    struct { halp_meta(name, "Scene Out"); ossia::scene_spec scene; uint8_t dirty{0}; } scene_out;
  } outputs;

  void rebuild();
  void operator()();

  void init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res);
  void update(
      score::gfx::RenderList& r, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e);
  void release(score::gfx::RenderList& r);

  std::shared_ptr<const ossia::scene_state> m_cached_out;
  uint8_t m_pending_dirty{0xFF};
  const ossia::scene_state* m_cached_in[4]{};
  int64_t m_cached_ver[4]{-1, -1, -1, -1};
  int64_t m_version_counter{0};

  score::gfx::GpuResourceRegistry::Slot raw_transform_slot;
  ossia::gpu_slot_ref m_xform_ref{};
};

}
