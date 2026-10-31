#pragma once
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <Gfx/Calibration/OffAxis.hpp>
#include <Gfx/Graph/CameraMath.hpp>
#include <Gfx/Graph/GpuResourceRegistry.hpp>

#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

class QRhiResourceUpdateBatch;

namespace score::gfx
{
class RenderList;
struct Edge;
}

namespace Threedim
{

// Local -Z points from eye to target; reference up is +Y, or -Z / +Z when
// looking straight down / up; roll_degrees turns around the view axis.
inline QQuaternion
cameraOrientation(QVector3D eye, QVector3D target, float roll_degrees) noexcept
{
  const QQuaternion roll
      = QQuaternion::fromAxisAndAngle(0.f, 0.f, 1.f, roll_degrees);
  QVector3D back = eye - target;
  if(back.lengthSquared() <= 1e-8f)
    return roll;
  back.normalize();

  QVector3D up(0.f, 1.f, 0.f);
  QVector3D right = QVector3D::crossProduct(up, back);
  if(right.lengthSquared() < 1e-12f)
  {
    up = QVector3D(0.f, 0.f, back.y() > 0.f ? -1.f : 1.f);
    right = QVector3D::crossProduct(up, back);
  }
  right.normalize();
  const QVector3D trueUp = QVector3D::crossProduct(back, right);
  return QQuaternion::fromAxes(right, trueUp, back) * roll;
}

inline std::array<float, 16> parseMatrix(std::string_view text) noexcept
{
  std::array<float, 16> m{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  std::array<float, 16> r{};
  int n = 0;
  const char* p = text.data();
  const char* end = p + text.size();
  while(p < end && n < 16)
  {
    if(!(std::isdigit((unsigned char)*p) || *p == '-' || *p == '+' || *p == '.'))
    {
      ++p;
      continue;
    }
    float v{};
    auto [next, ec] = std::from_chars(p, end, v);
    if(ec != std::errc{})
    {
      ++p;
      continue;
    }
    r[n++] = v;
    p = next;
  }
  return n == 16 ? r : m;
}

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
  halp_meta(description, "Create a scene camera with eye, target, field-of-view and clipping controls.")
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
      Eye() { value = {1.f, 1.f, 1.f}; }
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
    struct : halp::hslider_f32<"Roll", halp::range{-180., 180., 0.}>
    { void update(Camera& n) { n.rebuild(); } } roll;

    enum ProjectionMode
    {
      FieldOfView,
      Intrinsics,
      ProjectionMatrix,
      Screen
    };
    struct : halp::enum_t<ProjectionMode, "Projection">
    {
      struct range
      {
        std::string_view values[4]{"FOV", "Intrinsics", "Matrix", "Screen"};
        ProjectionMode init{FieldOfView};
      };
      void update(Camera& n) { n.rebuild(); }
    } projection;

    struct : halp::xy_spinboxes_f32<"Lens shift", halp::range{-4., 4., 0.}>
    { void update(Camera& n) { n.rebuild(); } } shift;

    struct Intr : halp::xyzw_spinboxes_f32<"Intrinsics", halp::range{-100000., 100000., 0.}>
    {
      Intr() { value = {1000.f, 1000.f, 959.5f, 539.5f}; }
      void update(Camera& n) { n.rebuild(); }
    } intrinsics;

    struct ImageSize : halp::xy_spinboxes_f32<"Image size", halp::range{1., 65536., 1.}>
    {
      ImageSize() { value = {1920.f, 1080.f}; }
      void update(Camera& n) { n.rebuild(); }
    } image_size;

    enum PoseMode
    {
      LookAt,
      WorldFromCameraCV,
      WorldFromCameraGL,
      ViewGL
    };
    struct : halp::enum_t<PoseMode, "Pose">
    {
      struct range
      {
        std::string_view values[4]{
            "Look at", "World from camera (OpenCV)", "World from camera (OpenGL)",
            "View (OpenGL)"};
        PoseMode init{LookAt};
      };
      void update(Camera& n) { n.rebuild(); }
    } pose;

    struct : halp::lineedit<"Pose matrix", "1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1">
    { void update(Camera& n) { n.rebuild(); } } pose_matrix;

    struct : halp::lineedit<"Projection matrix", "1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1">
    { void update(Camera& n) { n.rebuild(); } } projection_matrix;

    //! Screen projection: the screen's lower-left, lower-right and upper-left
    //! corners in world space, seen from Eye (Kooima 2008).
    struct ScreenA : halp::xyz_spinboxes_f32<"Screen lower left", halp::range{-10000., 10000., 0.}>
    {
      ScreenA() { value = {-1.f, 0.f, -1.f}; }
      void update(Camera& n) { n.rebuild(); }
    } screen_pa;
    struct ScreenB : halp::xyz_spinboxes_f32<"Screen lower right", halp::range{-10000., 10000., 0.}>
    {
      ScreenB() { value = {1.f, 0.f, -1.f}; }
      void update(Camera& n) { n.rebuild(); }
    } screen_pb;
    struct ScreenC : halp::xyz_spinboxes_f32<"Screen upper left", halp::range{-10000., 10000., 0.}>
    {
      ScreenC() { value = {-1.f, 2.f, -1.f}; }
      void update(Camera& n) { n.rebuild(); }
    } screen_pc;
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
    fillProjection(*cam);
    // Propagate the RawCamera arena slot ref (populated in init()).
    cam->raw_slot = m_camera_ref;

    // Encode the world transform as TRS for the scene_transform payload.
    ossia::scene_transform xform;
    xform.stable_id = m_xform_stable_id;
    const QVector3D t = position();
    xform.translation[0] = t.x();
    xform.translation[1] = t.y();
    xform.translation[2] = t.z();
    const QQuaternion q = orientation();
    xform.rotation[0] = q.x();
    xform.rotation[1] = q.y();
    xform.rotation[2] = q.z();
    xform.rotation[3] = q.scalar();
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

  void fillProjection(ossia::camera_component& cam) const noexcept
  {
    cam.projection = ossia::camera_projection::perspective;
    cam.yfov = inputs.fov.value * float(M_PI) / 180.f;
    cam.znear = inputs.near_plane.value;
    cam.zfar = inputs.far_plane.value;
    switch(inputs.projection.value)
    {
      case ins::FieldOfView:
        cam.shift[0] = inputs.shift.value.x;
        cam.shift[1] = inputs.shift.value.y;
        break;
      case ins::Intrinsics:
      {
        const auto& k = inputs.intrinsics.value;
        const float w = inputs.image_size.value.x;
        const float h = inputs.image_size.value.y;
        const auto f = score::gfx::frustumFromIntrinsics(k.x, k.y, k.z, k.w, w, h);
        cam.frustum[0] = f.left;
        cam.frustum[1] = f.right;
        cam.frustum[2] = f.bottom;
        cam.frustum[3] = f.top;
        cam.frustum_set = true;
        if(h > 0.f && k.y != 0.f)
        {
          cam.yfov = 2.f * std::atan(h / (2.f * std::abs(k.y)));
          cam.aspect_ratio = w / h;
          cam.aspect_ratio_set = true;
        }
        break;
      }
      case ins::ProjectionMatrix:
      {
        const auto pm = parseMatrix(inputs.projection_matrix.value);
        std::copy_n(pm.begin(), 16, cam.projection_matrix);
      }
        cam.projection_matrix_set = true;
        break;
      case ins::Screen:
      {
        const auto f = screenFrustum();
        if(!f.ok)
          break;
        cam.frustum[0] = float(f.l);
        cam.frustum[1] = float(f.r);
        cam.frustum[2] = float(f.b);
        cam.frustum[3] = float(f.t);
        cam.frustum_set = true;
        cam.yfov = float(std::atan(f.t) - std::atan(f.b));
        cam.aspect_ratio = float((f.r - f.l) / (f.t - f.b));
        cam.aspect_ratio_set = true;
        break;
      }
    }
  }

  bool screenMode() const noexcept { return inputs.projection.value == ins::Screen; }

  score::gfx::offaxis::Frustum screenFrustum() const noexcept
  {
    auto v = [](const auto& p) {
      return score::gfx::offaxis::Vec3(p.value.x, p.value.y, p.value.z);
    };
    return score::gfx::offaxis::kooima(
        v(inputs.screen_pa), v(inputs.screen_pb), v(inputs.screen_pc), v(inputs.eye));
  }

  //! The camera's world transform in score's frame (local -Z forward, +Y up).
  QMatrix4x4 worldTransform() const noexcept
  {
    QMatrix4x4 m;
    if(screenMode())
    {
      m.translate(QVector3D(inputs.eye.value.x, inputs.eye.value.y, inputs.eye.value.z));
      m.rotate(orientation());
      return m;
    }
    if(inputs.pose.value == ins::LookAt)
    {
      m.translate(
          QVector3D(inputs.eye.value.x, inputs.eye.value.y, inputs.eye.value.z));
      m.rotate(cameraOrientation(
          QVector3D(inputs.eye.value.x, inputs.eye.value.y, inputs.eye.value.z),
          QVector3D(
              inputs.target.value.x, inputs.target.value.y, inputs.target.value.z),
          inputs.roll.value));
      return m;
    }

    const auto pm = parseMatrix(inputs.pose_matrix.value);
    std::copy_n(pm.begin(), 16, m.data());
    switch(inputs.pose.value)
    {
      case ins::WorldFromCameraCV:
        m.scale(1.f, -1.f, -1.f);
        break;
      case ins::ViewGL:
        m = m.inverted();
        break;
      default:
        break;
    }
    return m;
  }

  QVector3D position() const noexcept
  {
    if(inputs.pose.value == ins::LookAt || screenMode())
      return QVector3D(inputs.eye.value.x, inputs.eye.value.y, inputs.eye.value.z);
    return worldTransform().column(3).toVector3D();
  }

  QQuaternion orientation() const noexcept
  {
    if(screenMode())
    {
      const auto f = screenFrustum();
      if(!f.ok)
        return {};
      auto q = [](const score::gfx::offaxis::Vec3& v) {
        return QVector3D(float(v.x()), float(v.y()), float(v.z()));
      };
      return QQuaternion::fromAxes(q(f.vr), q(f.vu), q(f.vn)).normalized();
    }
    if(inputs.pose.value == ins::LookAt)
      return cameraOrientation(
          QVector3D(inputs.eye.value.x, inputs.eye.value.y, inputs.eye.value.z),
          QVector3D(
              inputs.target.value.x, inputs.target.value.y, inputs.target.value.z),
          inputs.roll.value);

    const QMatrix4x4 m = worldTransform();
    QVector3D x = m.column(0).toVector3D();
    QVector3D y = m.column(1).toVector3D();
    QVector3D z = m.column(2).toVector3D();
    if(x.lengthSquared() < 1e-12f || y.lengthSquared() < 1e-12f
       || z.lengthSquared() < 1e-12f)
      return {};
    return QQuaternion::fromAxes(x.normalized(), y.normalized(), z.normalized())
        .normalized();
  }

  void operator()()
  {
    if(!m_state)
      rebuild();
    outputs.scene_out.scene.state = m_state;
    outputs.scene_out.dirty = m_pending_dirty;
    m_pending_dirty = 0;
  }

  // Render-thread hooks. init claims a RawCamera and a RawTransform slot;
  // update packs eye / target / up / yfov / znear / zfar into a
  // RawCameraData and uploads; release returns the slots.
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
