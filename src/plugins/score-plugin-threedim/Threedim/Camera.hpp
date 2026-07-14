#pragma once
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <Gfx/Graph/GpuResourceRegistry.hpp>

#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector3D>

#include <cmath>
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

// Scene-producing camera node. Emits a scene_spec containing:
//   - a scene_node with an id derived from this node's uuid (so the flatten
//     visitor can attribute the camera back to it),
//   - a scene_transform placing the camera at eye looking at target,
//   - a camera_component carrying yfov / znear / zfar.
//
// ScenePreprocessor packs every camera it collects into its Camera UBO
// output — when merged with a scene tree this camera becomes one entry in
// that array. active_camera_id defaults to this node's id so a single
// Camera is picked up automatically.
class Camera
{
public:
  halp_meta(name, "Camera")
  halp_meta(c_name, "camera_avnd")
  halp_meta(category, "Visuals/3D/Scene")
  halp_meta(authors, "ossia team")
  halp_meta(uuid, "4c91b5e2-8d76-4ab3-9f14-6e0d8b3a2c57")

  struct ins
  {
    // Port-driven rebuild: every control carries an `update(Camera&)`
    // callback that fires only when its value changes, triggering a
    // `rebuild()` on the Camera. `operator()()` then just republishes
    // the already-built m_state — no per-frame memcmp, no per-frame
    // version bump, no merge_scenes / preprocessor thrash.
    //
    // halp::range only supports scalar inits (broadcast across x/y/z), so
    // the non-uniform defaults are applied in the subclass constructor.
    struct Eye : halp::xyz_spinboxes_f32<"Eye", halp::range{-10000., 10000., 0.}>
    {
      Eye() { value = {0.f, 1.f, 3.f}; }
      void update(Camera& n) { n.rebuild(); }
    } eye;
    struct : halp::xyz_spinboxes_f32<"Target", halp::range{-10000., 10000., 0.}>
    { void update(Camera& n) { n.rebuild(); } } target;
    struct : halp::hslider_f32<"FOV", halp::range{5., 170., 60.}>
    { void update(Camera& n) { n.rebuild(); } } fov;
    struct : halp::hslider_f32<"Near", halp::range{0.001, 10., 0.1}>
    { void update(Camera& n) { n.rebuild(); } } near_plane;
    struct : halp::hslider_f32<"Far", halp::range{1., 100000., 1000.}>
    { void update(Camera& n) { n.rebuild(); } } far_plane;
  } inputs;

  struct outs
  {
    struct
    {
      halp_meta(name, "Scene");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene_out;
  } outputs;

  // Stable scene_node_id for this camera across frames. Set once in the
  // first call. Used as scene_state::active_camera_id so ScenePreprocessor
  // picks THIS camera even when other cameras show up in merged scenes.
  ossia::scene_node_id m_id{};
  std::shared_ptr<ossia::scene_state> m_state;
  int64_t m_version{0};
  // Dirty bits to stamp on the next emission. Accumulated in rebuild()
  // and cleared after operator()() publishes them. When no control
  // changed this frame, operator()() republishes the same m_state with
  // dirty=0 so the preprocessor's pointer+version comparison short-
  // circuits the rebuild path.
  uint8_t m_pending_dirty{ossia::scene_port::dirty_transform};
  // Stable ids for the single scene_transform + camera_component this
  // node emits (minted on first rebuild).
  uint64_t m_xform_stable_id{};
  uint64_t m_camera_stable_id{};

  // Rebuild m_state from current inputs. Called from every port's
  // `update()` callback (fires only on control changes), and once from
  // `operator()()` on the first tick to seed m_state.
  void rebuild()
  {
    if(!m_state)
    {
      m_state = std::make_shared<ossia::scene_state>();
      // Deterministic, non-zero id keyed on this node's address. Non-zero
      // so merge_scenes' active_camera_id resolution treats it as "set".
      m_id.value = reinterpret_cast<std::uintptr_t>(this) | 0x1u;
    }
    if(m_camera_stable_id == 0) m_camera_stable_id = ossia::mint_stable_id();
    if(m_xform_stable_id == 0) m_xform_stable_id = ossia::mint_stable_id();

    // Rebuild as {scene_transform, camera_component} inside a scene_node.
    auto cam = std::make_shared<ossia::camera_component>();
    cam->stable_id = m_camera_stable_id;
    cam->projection = ossia::camera_projection::perspective;
    cam->yfov = inputs.fov.value * float(M_PI) / 180.f;
    cam->znear = inputs.near_plane.value;
    cam->zfar = inputs.far_plane.value;
    // Propagate the RawCamera arena slot ref (populated in init()).
    cam->raw_slot = m_camera_ref;

    // Encode the world transform as TRS for the scene_transform payload.
    ossia::scene_transform xform;
    xform.stable_id = m_xform_stable_id;
    xform.translation[0] = inputs.eye.value.x;
    xform.translation[1] = inputs.eye.value.y;
    xform.translation[2] = inputs.eye.value.z;
    // Build a quaternion for the camera's world orientation. Qt's
    // QQuaternion::fromDirection(direction, up) maps local +Z (NOT -Z) to
    // `direction` — see QMatrix4x4::fromAxes in Qt source, which takes
    // zAxis = direction. We want the camera's local +Z axis (the "back"
    // axis of a GL camera) to point along (eye − target) so that local -Z
    // (the GL viewing direction) points from eye toward target. Hence the
    // -forward. Equivalently: the inverse of the TRS matches
    // QMatrix4x4::lookAt(eye, target, up).
    QVector3D forward(
        inputs.target.value.x - inputs.eye.value.x,
        inputs.target.value.y - inputs.eye.value.y,
        inputs.target.value.z - inputs.eye.value.z);
    if(forward.lengthSquared() > 1e-8f)
    {
      forward.normalize();
      QQuaternion q = QQuaternion::fromDirection(
          -forward, QVector3D(0.f, 1.f, 0.f));
      xform.rotation[0] = q.x();
      xform.rotation[1] = q.y();
      xform.rotation[2] = q.z();
      xform.rotation[3] = q.scalar();
    }
    else
    {
      xform.rotation[0] = 0.f;
      xform.rotation[1] = 0.f;
      xform.rotation[2] = 0.f;
      xform.rotation[3] = 1.f;
    }
    xform.scale[0] = 1.f;
    xform.scale[1] = 1.f;
    xform.scale[2] = 1.f;
    // Propagate the RawTransform slot ref (populated in init()).
    xform.raw_slot = m_xform_ref;

    auto children = std::make_shared<std::vector<ossia::scene_payload>>();
    children->push_back(xform);
    children->push_back(ossia::camera_component_ptr(std::move(cam)));

    auto node = std::make_shared<ossia::scene_node>();
    node->id = m_id;
    node->children = std::move(children);

    auto roots
        = std::make_shared<std::vector<ossia::scene_node_ptr>>();
    roots->push_back(std::move(node));

    m_state->roots = std::move(roots);
    m_state->active_camera_id = m_id;
    m_version++;
    m_state->version = m_version;
    m_pending_dirty = ossia::scene_port::dirty_transform;
  }

  void operator()()
  {
    if(!m_state)
      rebuild();
    outputs.scene_out.scene.state = m_state;
    outputs.scene_out.dirty = m_pending_dirty;
    m_pending_dirty = 0;
  }

  // Render-thread hooks. init claims one RawCamera slot; update packs
  // eye / target / up / yfov / znear / zfar into a RawCameraData and
  // uploads; release returns the slot. The preprocessor will consume
  // the slot in a later pass (aspect-ratio-aware matrix composition
  // happens there); for now the scene_spec emission still drives
  // packAndUploadCameras and this slot is a producer-half plumbing.
  void init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res);
  void update(
      score::gfx::RenderList& r, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e);
  void release(score::gfx::RenderList& r);

  score::gfx::GpuResourceRegistry::Slot raw_camera_slot;
  score::gfx::GpuResourceRegistry::Slot raw_transform_slot;

  // Ossia-facing snapshots, stamped on the emitted components'
  // raw_slot fields so the preprocessor can locate this camera's
  // GPU bytes via isLive() + offset. Written once in init().
  ossia::gpu_slot_ref m_camera_ref{};
  ossia::gpu_slot_ref m_xform_ref{};
};

}
