#pragma once
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <Gfx/Graph/GpuResourceRegistry.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>  // sizeof(score::gfx::RawCameraData) in operator()()

#include <QQuaternion>
#include <QVector3D>

#include <array>
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

// Scene-producing node that emits a six-camera array laid out for cubemap
// / multiview rendering. Each camera is a scene_node with a
// scene_transform + camera_component payload; ScenePreprocessor's flatten
// visitor picks them up into FlatScene::cameras, and
// packAndUploadCameras packs them into the Camera UBO aux-buffer on
// Geometry Out. Multiview shaders (MULTIVIEW=6) then index camera[0..5]
// via gl_ViewIndex.
//
// Face convention follows the GL cubemap layout:
//   camera[0] = +X, [1] = -X, [2] = +Y, [3] = -Y, [4] = +Z, [5] = -Z
// Each face uses a 90° square FOV with aspect 1:1 — consumers should
// render into a cube render target at any square resolution.
class CameraArray
{
public:
  halp_meta(name, "Camera Array")
  halp_meta(c_name, "camera_array_avnd")
  halp_meta(category, "Visuals/3D/Scene")
  halp_meta(authors, "ossia team")
  halp_meta(uuid, "7a3e8d2f-1b94-4c6a-b7f5-8e2d0c1a4b93")

  // Six GL-ordered cubemap faces at 90° FoV, aspect 1:1. Suitable as
  // both a reflection probe array and a point-shadow cube array — the
  // distinction is downstream (which render target / depth-only flag),
  // not in the camera math here.
  struct ins
  {
    // Port-driven rebuild: each control's update() callback fires
    // CameraArray::rebuild() on change. operator()() republishes.
    struct : halp::xyz_spinboxes_f32<"Origin", halp::range{-10000., 10000., 0.}>
    { void update(CameraArray& n) { n.rebuild(); } } origin;
    struct : halp::hslider_f32<"Near", halp::range{0.001, 10., 0.1}>
    { void update(CameraArray& n) { n.rebuild(); } } near_plane;
    struct : halp::hslider_f32<"Far", halp::range{1., 100000., 1000.}>
    { void update(CameraArray& n) { n.rebuild(); } } far_plane;
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

  // Canonical cubemap face orientations in the GL convention:
  // { forward, up }. right = forward × up.
  struct Face
  {
    float forward[3];
    float up[3];
  };

  // Six deterministic ids rooted at this node's address — each face
  // needs a stable, distinct scene_node_id so merge_scenes treats them
  // as six separate cameras (same-id camera entries would collapse).
  std::array<ossia::scene_node_id, 6> m_ids{};
  std::shared_ptr<ossia::scene_state> m_state;
  int64_t m_version{0};
  uint8_t m_pending_dirty{ossia::scene_port::dirty_transform};

  void rebuild()
  {
    if(!m_state)
    {
      m_state = std::make_shared<ossia::scene_state>();
      // Seed six distinct ids from this node's address. OR the per-face
      // index in so they're all non-zero AND all distinct.
      const auto base = reinterpret_cast<std::uintptr_t>(this);
      for(int i = 0; i < 6; ++i)
        m_ids[std::size_t(i)].value = (base ^ (std::uintptr_t(i + 1) << 1)) | 0x1u;
    }

    static constexpr std::array<Face, 6> kFaces{{
        {{ 1.f,  0.f,  0.f}, {0.f, -1.f,  0.f}},  // +X
        {{-1.f,  0.f,  0.f}, {0.f, -1.f,  0.f}},  // -X
        {{ 0.f,  1.f,  0.f}, {0.f,  0.f,  1.f}},  // +Y
        {{ 0.f, -1.f,  0.f}, {0.f,  0.f, -1.f}},  // -Y
        {{ 0.f,  0.f,  1.f}, {0.f, -1.f,  0.f}},  // +Z
        {{ 0.f,  0.f, -1.f}, {0.f, -1.f,  0.f}},  // -Z
    }};

    const float near_f = inputs.near_plane.value;
    const float far_f = inputs.far_plane.value;
    const float eye[3]
        = {inputs.origin.value.x, inputs.origin.value.y,
           inputs.origin.value.z};

    auto roots
        = std::make_shared<std::vector<ossia::scene_node_ptr>>();
    roots->reserve(6);

    for(int i = 0; i < 6; ++i)
    {
      auto cam = std::make_shared<ossia::camera_component>();
      cam->projection = ossia::camera_projection::perspective;
      cam->yfov = float(M_PI) / 2.f;  // 90° per face for a seamless cube
      cam->aspect_ratio = 1.f;
      cam->znear = near_f;
      cam->zfar = far_f;
      // Each face owns one RawCameraData inside our single 6-wide slot.
      // Stamp a derived ref with the face's offset — same arena /
      // internal_index / generation, offset bumped by i entries.
      if(m_array_ref.valid())
      {
        cam->raw_slot = m_array_ref;
        cam->raw_slot.offset = m_array_ref.offset
            + uint32_t(i * sizeof(score::gfx::RawCameraData));
        cam->raw_slot.size = uint32_t(sizeof(score::gfx::RawCameraData));
      }

      ossia::scene_transform xform;
      xform.translation[0] = eye[0];
      xform.translation[1] = eye[1];
      xform.translation[2] = eye[2];

      // Same rationale as Camera.hpp: Qt's QQuaternion::fromDirection
      // maps local +Z to `direction`, but GL cameras look along local -Z
      // — pass the negated forward so local -Z ends up pointing along
      // +forward (the face-direction).
      QVector3D fwd(
          kFaces[std::size_t(i)].forward[0], kFaces[std::size_t(i)].forward[1],
          kFaces[std::size_t(i)].forward[2]);
      QVector3D up(
          kFaces[std::size_t(i)].up[0], kFaces[std::size_t(i)].up[1],
          kFaces[std::size_t(i)].up[2]);
      QQuaternion q = QQuaternion::fromDirection(-fwd, up);
      xform.rotation[0] = q.x();
      xform.rotation[1] = q.y();
      xform.rotation[2] = q.z();
      xform.rotation[3] = q.scalar();
      xform.scale[0] = 1.f;
      xform.scale[1] = 1.f;
      xform.scale[2] = 1.f;
      // Per-face RawTransform slot ref — same shape as the camera
      // array ref, offset bumped to the i-th RawLocalTransform slot.
      if(m_xform_array_ref.valid())
      {
        xform.raw_slot = m_xform_array_ref;
        xform.raw_slot.offset = m_xform_array_ref.offset
            + uint32_t(i * sizeof(score::gfx::RawLocalTransform));
        xform.raw_slot.size
            = uint32_t(sizeof(score::gfx::RawLocalTransform));
      }

      auto children
          = std::make_shared<std::vector<ossia::scene_payload>>();
      children->push_back(xform);
      children->push_back(ossia::camera_component_ptr(std::move(cam)));

      auto node = std::make_shared<ossia::scene_node>();
      node->id = m_ids[std::size_t(i)];
      node->children = std::move(children);

      roots->push_back(std::move(node));
    }

    m_state->roots = std::move(roots);
    // Face 0 (+X) acts as the "active" camera for non-multiview consumers
    // that only read the first entry. Multiview shaders ignore this and
    // index all six via gl_ViewIndex.
    m_state->active_camera_id = m_ids[0];
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

  // Render-thread hooks. A single RawCamera slot holds all six faces
  // contiguously (6 × RawCameraData). The preprocessor will later
  // consume this slot and compose view/projection matrices for each
  // face with the target's aspect (1:1 for the cubemap case).
  void init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res);
  void update(
      score::gfx::RenderList& r, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e);
  void release(score::gfx::RenderList& r);

  score::gfx::GpuResourceRegistry::Slot raw_camera_slot;
  score::gfx::GpuResourceRegistry::Slot raw_transform_slot;

  // Ossia-facing base refs for our 6-wide RawCamera + 6-wide
  // RawTransform slots. Each emitted camera_component / scene_transform
  // gets these refs with its per-face offset bumped.
  ossia::gpu_slot_ref m_array_ref{};
  ossia::gpu_slot_ref m_xform_array_ref{};
};

}
