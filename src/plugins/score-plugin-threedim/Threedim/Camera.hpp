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
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#if !defined(SCORE_HAS_STD_FLOAT_FROM_CHARS)
#if defined(__cpp_lib_to_chars) && !defined(_LIBCPP_VERSION)
#define SCORE_HAS_STD_FLOAT_FROM_CHARS 1
#else
#define SCORE_HAS_STD_FLOAT_FROM_CHARS 0
#endif
#endif

#if !SCORE_HAS_STD_FLOAT_FROM_CHARS
#include <boost/lexical_cast/try_lexical_convert.hpp>
#endif

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

inline bool parseMatrixNumber(std::string_view token, float& out) noexcept
{
  if(!token.empty() && token.front() == '+')
    token.remove_prefix(1);
#if SCORE_HAS_STD_FLOAT_FROM_CHARS
  const auto [next, ec] = std::from_chars(token.data(), token.data() + token.size(), out);
  return ec == std::errc{} && next == token.data() + token.size();
#else
  return boost::conversion::try_lexical_convert(token.data(), token.size(), out);
#endif
}

//! 16 numbers written row by row, as a matrix is printed (any separators:
//! spaces, commas, brackets), stored column-major as QMatrix4x4::data().
//! Nothing when there are not exactly 16 numbers or one does not parse.
inline std::optional<std::array<float, 16>> parseMatrix(std::string_view text) noexcept
{
  std::array<float, 16> r{};
  int n = 0;
  auto isNumberChar = [](char c) {
    return std::isdigit((unsigned char)c) || c == '-' || c == '+' || c == '.' || c == 'e'
           || c == 'E';
  };
  std::size_t i = 0;
  while(i < text.size())
  {
    if(!isNumberChar(text[i]))
    {
      ++i;
      continue;
    }
    std::size_t j = i;
    while(j < text.size() && isNumberChar(text[j]))
      ++j;
    float v{};
    if(n == 16 || !parseMatrixNumber(text.substr(i, j - i), v) || !std::isfinite(v))
      return std::nullopt;
    r[(n % 4) * 4 + n / 4] = v;
    ++n;
    i = j;
  }
  if(n != 16)
    return std::nullopt;
  return r;
}

//! parseMatrix of a text, parsed again only when the text changes.
struct ParsedMatrix
{
  std::string text;
  std::optional<std::array<float, 16>> matrix;
  bool parsed{};

  const std::optional<std::array<float, 16>>& operator()(const std::string& t)
  {
    if(!parsed || t != text)
    {
      text = t;
      matrix = parseMatrix(t);
      parsed = true;
    }
    return matrix;
  }
};

// Scene-producing camera node. Emits a scene_spec containing:
//   - a scene_node with an id derived from this node's uuid (so the flatten
//     visitor can attribute the camera back to it),
//   - a scene_transform: eye looking at target, a pose matrix, or the
//     orientation of a screen (Screen projection),
//   - a camera_component: FOV and lens shift, OpenCV intrinsics, a raw
//     OpenGL projection matrix, or the off-axis frustum of a screen.
// A pose or projection matrix that does not parse, a pose that is not affine,
// non-positive focal lengths or a degenerate screen fall back to Look at and
// FOV.
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
  halp_meta(description, "Create a scene camera: look-at, FOV and lens shift, OpenCV intrinsics, raw OpenGL matrices (row by row) or a screen seen from the eye.")
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

    struct : halp::lineedit<"Pose matrix (rows)", "1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1">
    { void update(Camera& n) { n.rebuild(); } } pose_matrix;

    struct : halp::lineedit<"Projection matrix (rows)", "1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1">
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
    fillProjection(*cam);
    m_yfov = cam->yfov;
    m_znear = cam->znear;
    m_zfar = cam->zfar;
    // Propagate the RawCamera arena slot ref (populated in init()).
    cam->raw_slot = m_camera_ref;

    // Encode the world transform as TRS for the scene_transform payload.
    ossia::scene_transform xform;
    xform.stable_id = m_xform_stable_id;
    const QVector3D t = position();
    m_position = t;
    xform.translation[0] = t.x();
    xform.translation[1] = t.y();
    xform.translation[2] = t.z();
    const QQuaternion q = orientation();
    m_orientation = q;
    m_target = !poseMatrix() && !(screenMode() && cam->frustum_set)
                   ? QVector3D(
                       inputs.target.value.x, inputs.target.value.y,
                       inputs.target.value.z)
                   : t + q.rotatedVector(QVector3D(0.f, 0.f, -1.f));
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
    auto fov = [&] {
      cam.shift[0] = inputs.shift.value.x;
      cam.shift[1] = inputs.shift.value.y;
    };
    switch(inputs.projection.value)
    {
      case ins::FieldOfView:
        fov();
        break;
      case ins::Intrinsics:
      {
        const auto& k = inputs.intrinsics.value;
        const float w = inputs.image_size.value.x;
        const float h = inputs.image_size.value.y;
        if(!(k.x > 0.f) || !(k.y > 0.f) || !(w > 0.f) || !(h > 0.f))
        {
          fov();
          break;
        }
        const auto f = score::gfx::frustumFromIntrinsics(k.x, k.y, k.z, k.w, w, h);
        cam.frustum[0] = f.left;
        cam.frustum[1] = f.right;
        cam.frustum[2] = f.bottom;
        cam.frustum[3] = f.top;
        cam.frustum_set = true;
        cam.yfov = 2.f * std::atan(h / (2.f * k.y));
        cam.aspect_ratio = w / h;
        cam.aspect_ratio_set = true;
        break;
      }
      case ins::ProjectionMatrix:
      {
        const auto& pm = m_projectionMatrix(inputs.projection_matrix.value);
        if(!pm)
        {
          fov();
          break;
        }
        std::copy_n(pm->begin(), 16, cam.projection_matrix);
        cam.projection_matrix_set = true;
        matrixPlanes(*pm, cam);
        break;
      }
      case ins::Screen:
      {
        const auto f = screenFrustum();
        if(!f.ok)
        {
          fov();
          break;
        }
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

  //! The planes, kind and vertical FOV of an OpenGL projection matrix
  //! (column-major), so what the camera reports matches what it renders.
  static void
  matrixPlanes(const std::array<float, 16>& m, ossia::camera_component& cam) noexcept
  {
    const float m00 = m[0], m11 = m[5], m22 = m[10], m23 = m[14], m32 = m[11],
                m33 = m[15];
    float n{}, f{};
    if(m32 == 0.f && m33 == 1.f)
    {
      if(m22 == 0.f)
        return;
      n = (m23 + 1.f) / m22;
      f = (m23 - 1.f) / m22;
      if(!std::isfinite(n) || !std::isfinite(f) || n == f)
        return;
      cam.projection = ossia::camera_projection::orthographic;
      if(m00 != 0.f && m11 != 0.f)
      {
        cam.xmag = 1.f / std::abs(m00);
        cam.ymag = 1.f / std::abs(m11);
      }
    }
    else
    {
      n = m23 / (m22 - 1.f);
      f = m23 / (m22 + 1.f);
      if(!(n > 0.f) || !std::isfinite(f) || n == f)
        return;
      if(m11 != 0.f)
        cam.yfov = 2.f * std::atan(1.f / std::abs(m11));
    }
    cam.znear = n;
    cam.zfar = f;
  }

  bool screenMode() const noexcept { return inputs.projection.value == ins::Screen; }

  //! Screen mode with a solvable screen: the eye in front of non-degenerate
  //! corners. Otherwise the camera falls back to Look at and FOV.
  bool screenValid() const noexcept { return screenMode() && screenFrustum().ok; }

  score::gfx::offaxis::Frustum screenFrustum() const noexcept
  {
    auto v = [](const auto& p) {
      return score::gfx::offaxis::Vec3(p.value.x, p.value.y, p.value.z);
    };
    return score::gfx::offaxis::kooima(
        v(inputs.screen_pa), v(inputs.screen_pb), v(inputs.screen_pc), v(inputs.eye));
  }

  //! The pose matrix, when the pose comes from one that parses and is affine.
  const std::array<float, 16>* poseMatrix() const noexcept
  {
    if(screenMode() || inputs.pose.value == ins::LookAt)
      return nullptr;
    const auto& pm = m_poseMatrix(inputs.pose_matrix.value);
    if(!pm || (*pm)[3] != 0.f || (*pm)[7] != 0.f || (*pm)[11] != 0.f || (*pm)[15] != 1.f)
      return nullptr;
    return &*pm;
  }

  //! The camera's world transform in score's frame (local -Z forward, +Y up).
  QMatrix4x4 worldTransform() const noexcept
  {
    QMatrix4x4 m;
    const auto* pm = poseMatrix();
    if(!pm)
    {
      m.translate(
          QVector3D(inputs.eye.value.x, inputs.eye.value.y, inputs.eye.value.z));
      m.rotate(orientation());
      return m;
    }

    std::copy_n(pm->begin(), 16, m.data());
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
    if(!poseMatrix())
      return QVector3D(inputs.eye.value.x, inputs.eye.value.y, inputs.eye.value.z);
    return worldTransform().column(3).toVector3D();
  }

  QQuaternion orientation() const noexcept
  {
    if(screenMode())
    {
      if(const auto f = screenFrustum(); f.ok)
      {
        auto q = [](const score::gfx::offaxis::Vec3& v) {
          return QVector3D(float(v.x()), float(v.y()), float(v.z()));
        };
        return QQuaternion::fromAxes(q(f.vr), q(f.vu), q(f.vn)).normalized();
      }
    }
    if(!poseMatrix())
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

  mutable ParsedMatrix m_poseMatrix, m_projectionMatrix;

  //! What rebuild() computed, for update() on every frame.
  QVector3D m_position, m_target;
  QQuaternion m_orientation;
  float m_yfov{}, m_znear{}, m_zfar{};
};

}
