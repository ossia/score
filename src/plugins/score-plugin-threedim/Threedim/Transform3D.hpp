#pragma once
#include "TransformHelper.hpp"

#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <Gfx/Graph/GpuResourceRegistry.hpp>

#include <cstdint>
#include <memory>
#include <vector>

class QRhiResourceUpdateBatch;

namespace score::gfx
{
class RenderList;
struct Edge;
}

namespace Threedim
{

// Scene-in → scene-out transform: wraps the incoming scene's roots under a
// single parent node carrying a `scene_transform` payload (TRS). Materials,
// animations and cameras pass through by shared_ptr identity so downstream
// identity-based caches stay hot.
class Transform3D
{
public:
  halp_meta(name, "Transform 3D")
  halp_meta(c_name, "transform3d_avnd")
  halp_meta(category, "Visuals/3D/Scene")
  halp_meta(authors, "ossia team")
  halp_meta(uuid, "7a9f2b41-4d58-4e93-b7c2-0f5d3e8a6b1c")

  struct ins
  {
    struct
    {
      halp_meta(name, "Scene In");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene_in;

    halp::xyz_spinboxes_f32<
        "Position", halp::range{-10000., 10000., 0.}>
        position;
    halp::xyz_spinboxes_f32<"Rotation", halp::range{0., 359.9999999, 0.}>
        rotation;
    halp::xyz_spinboxes_f32<
        "Scale", halp::range{0.00001, 1000., 1.}>
        scale;
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

  void operator()();

  // Render-thread hooks. init claims one RawTransform slot for the
  // emitted scene_transform; update packs the current control TRS
  // into a RawLocalTransform and uploads; release returns the slot.
  // The preprocessor composes the world-space matrix for this slot
  // from the scene-node parent chain CPU-side.
  void init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res);
  void update(
      score::gfx::RenderList& r, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e);
  void release(score::gfx::RenderList& r);

  score::gfx::GpuResourceRegistry::Slot xform_slot;

  // Ossia-facing snapshot of xform_slot. Written once in init(),
  // copied onto the emitted scene_transform's raw_slot every
  // operator()() tick.
  ossia::gpu_slot_ref m_xform_ref{};

  // Cache: republish the same emitted scene_state when neither upstream
  // (input scene_state pointer / version) nor controls (TRS) changed.
  // Prevents downstream SceneSelector / SceneGraphFilter / SceneDuplicator /
  // CreateCollection from rebuilding every frame, which they did when we
  // emitted a fresh shared_ptr each tick — diagnostic 027.
  std::shared_ptr<const ossia::scene_state> m_state;
  const ossia::scene_state* m_cached_in_state{};
  int64_t m_cached_in_version{-1};
  CachedTRS m_cachedTRS{};
  int64_t m_version_counter{0};
};

}
