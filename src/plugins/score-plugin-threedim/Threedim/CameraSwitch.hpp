#pragma once
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QQuaternion>
#include <QVector3D>

#include <cmath>
#include <cstdint>
#include <memory>

namespace Threedim
{

// 4-way camera switch + weighted blender.
//
// Operates at the scene_spec level like SceneSwitch but specialised to a
// single purpose: select or blend between up to 4 Camera producers. Each
// input is expected to be the output of a `Threedim::Camera` node (or any
// scene_spec whose first root carries a scene_transform + camera_component
// pair).
//
// Modes:
//   - Select: the `index` parameter picks one of the four inputs; the other
//             three are ignored. Equivalent to dropping SceneSwitch in front
//             of a camera, but avoids the caveat that non-camera scene data
//             from the unselected inputs would get dropped too.
//   - Blend : the `weights` (x,y,z,w) parameter linearly blends the
//             positions + FOV + near/far of the four inputs, normalise-lerps
//             (nlerp) the orientation quaternions. Weights are auto-
//             normalised to sum=1 internally — users can pass raw
//             envelopes / LFO outputs directly.
//
// Blend semantics chosen to match what TD's Camera Blend COMP does
// conceptually: treat each input camera as a "keyframe pose" and produce
// a smooth in-between. nlerp is fine for small angular deltas; when you
// need great-circle blending across wide angles, upgrade to slerp (two
// slerps for 4-way is the standard recipe).
//
// Unwired inputs fall back to a zero-weight contribution. When all wired
// inputs have zero effective weight the output is empty.
class CameraSwitch
{
public:
  halp_meta(name, "Camera Switch")
  halp_meta(category, "Visuals/3D/Scene")
  halp_meta(c_name, "camera_switch")
  halp_meta(authors, "ossia team")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/camera-switch.html")
  halp_meta(uuid, "d1e8c4b7-6a32-4f9e-b5d8-2c4f3a1e8b6d")

  struct ins
  {
    struct
    {
      halp_meta(name, "Camera 0");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } cam0;
    struct
    {
      halp_meta(name, "Camera 1");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } cam1;
    struct
    {
      halp_meta(name, "Camera 2");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } cam2;
    struct
    {
      halp_meta(name, "Camera 3");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } cam3;

    enum CameraMode
    {
      Select,
      Blend
    };
    // Port-driven rebuild: controls trigger CameraSwitch::rebuild().
    // Upstream camera-input changes are detected in operator()().
    struct Mode : halp::enum_t<CameraMode, "Mode">
    {
      struct range
      {
        std::string_view values[2]{"Select", "Blend"};
        CameraMode init{Select};
      };
      void update(CameraSwitch& n) { n.rebuild(); }
    } mode;

    struct : halp::spinbox_i32<"Index", halp::irange{0, 3, 0}>
    { void update(CameraSwitch& n) { n.rebuild(); } } index;

    // Four-channel blend weights. Negative values are clamped to zero.
    struct : halp::xyzw_spinboxes_f32<"Weights", halp::range{-10000., 10000., 0.}>
    { void update(CameraSwitch& n) { n.rebuild(); } } weights;
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

  // Stable id for the synthesised camera in Blend mode. One id kept for the
  // whole life of the node so downstream preprocessor logic treats frames
  // as updates to the same camera rather than a sequence of add/remove
  // events.
  ossia::scene_node_id m_id{};
  std::shared_ptr<ossia::scene_state> m_state;
  int64_t m_version{0};
  uint8_t m_pending_dirty{ossia::scene_port::dirty_transform};
  // Cached upstream identity for detecting scene_in pointer/version
  // changes from within the new 5-line operator()() republish path.
  const ossia::scene_state* m_cached_cam_state[4]{};
  int64_t m_cached_cam_ver[4]{-1, -1, -1, -1};

  // Locate the first (scene_transform, camera_component) pair in a scene.
  // Returns false if the input has no camera (or is empty).
  static bool extractCameraPose(
      const ossia::scene_spec& in, ossia::scene_transform& xform,
      ossia::camera_component& cam)
  {
    if(!in.state || !in.state->roots || in.state->roots->empty())
      return false;
    const auto& root = (*in.state->roots)[0];
    if(!root || !root->children)
      return false;

    bool gotXform = false;
    bool gotCam   = false;
    for(const auto& child : *root->children)
    {
      if(auto* t = ossia::get_if<ossia::scene_transform>(&child))
      {
        xform = *t;
        gotXform = true;
      }
      else if(auto* c = ossia::get_if<ossia::camera_component_ptr>(&child))
      {
        if(*c)
        {
          cam = **c;
          gotCam = true;
        }
      }
    }
    return gotXform && gotCam;
  }

  void rebuild()
  {
    const int mode = inputs.mode.value;
    if(mode == ins::CameraMode::Select)
    {
      // Select-mode: operator()() forwards the picked upstream
      // scene_spec directly; rebuild() just marks pending dirty so
      // downstream sees a transition event.
      m_pending_dirty = 0xFF;
      return;
    }

    // Blend mode.
    float w[4]{
        inputs.weights.value.x, inputs.weights.value.y,
        inputs.weights.value.z, inputs.weights.value.w};
    for(float& x : w) x = x > 0.f ? x : 0.f;

    const ossia::scene_spec* inputsArr[4]{
        &inputs.cam0.scene, &inputs.cam1.scene,
        &inputs.cam2.scene, &inputs.cam3.scene};

    // Extract each input's pose; zero the weight of any missing one.
    ossia::scene_transform xforms[4]{};
    ossia::camera_component cams[4]{};
    float effWeights[4]{};
    float wsum = 0.f;
    for(int i = 0; i < 4; ++i)
    {
      if(w[i] <= 0.f) continue;
      if(!extractCameraPose(*inputsArr[i], xforms[i], cams[i]))
        continue;
      effWeights[i] = w[i];
      wsum += w[i];
    }

    if(wsum <= 1e-6f)
    {
      // No wired-and-weighted camera to blend — emit empty.
      if(m_state)
      {
        m_state->roots.reset();
        m_state->active_camera_id = {};
        m_version++;
        m_state->version = m_version;
      }
      // Bump dirty so consumers (preprocessor cache, downstream
      // SceneSelector) detect the empty-state transition. Without
      // this they'd see the same shared_ptr identity + stale
      // version + dirty=0 and keep rendering last frame's blend.
      m_pending_dirty = 0xFF;
      return;
    }
    for(float& x : effWeights) x /= wsum;

    // Blend transform: translation is weighted sum; rotation is nlerp
    // (weighted sum of quaternions, then normalise); scale is weighted sum.
    ossia::scene_transform outX{};
    QQuaternion qSum(0, 0, 0, 0);
    for(int i = 0; i < 4; ++i)
    {
      if(effWeights[i] <= 0.f) continue;
      const float wi = effWeights[i];
      outX.translation[0] += xforms[i].translation[0] * wi;
      outX.translation[1] += xforms[i].translation[1] * wi;
      outX.translation[2] += xforms[i].translation[2] * wi;
      outX.scale[0]       += xforms[i].scale[0] * wi;
      outX.scale[1]       += xforms[i].scale[1] * wi;
      outX.scale[2]       += xforms[i].scale[2] * wi;

      // Quaternion double-cover handling: flip the sign of later quats if
      // they point away from the running sum, to avoid interpolating the
      // long way around.
      QQuaternion qi(
          xforms[i].rotation[3], xforms[i].rotation[0],
          xforms[i].rotation[1], xforms[i].rotation[2]);
      if(QQuaternion::dotProduct(qSum, qi) < 0.f)
        qi = -qi;
      qSum += qi * wi;
    }
    qSum.normalize();
    outX.rotation[0] = qSum.x();
    outX.rotation[1] = qSum.y();
    outX.rotation[2] = qSum.z();
    outX.rotation[3] = qSum.scalar();

    // Blend camera parameters.
    ossia::camera_component outCam{};
    outCam.projection = cams[0].projection; // projection mode not blendable
    for(int i = 0; i < 4; ++i)
    {
      if(effWeights[i] <= 0.f) continue;
      const float wi = effWeights[i];
      outCam.yfov         += cams[i].yfov         * wi;
      outCam.aspect_ratio += cams[i].aspect_ratio * wi;
      outCam.xmag         += cams[i].xmag         * wi;
      outCam.ymag         += cams[i].ymag         * wi;
      outCam.znear        += cams[i].znear        * wi;
      outCam.zfar         += cams[i].zfar         * wi;
      outCam.physical.focal_length   += cams[i].physical.focal_length   * wi;
      outCam.physical.focus_distance += cams[i].physical.focus_distance * wi;
      outCam.physical.fstop          += cams[i].physical.fstop          * wi;
    }

    // Build the output scene_spec.
    if(!m_state)
    {
      m_state = std::make_shared<ossia::scene_state>();
      m_id.value = reinterpret_cast<std::uintptr_t>(this) | 0x1u;
    }

    auto camPtr = std::make_shared<ossia::camera_component>(std::move(outCam));
    auto children = std::make_shared<std::vector<ossia::scene_payload>>();
    children->push_back(outX);
    children->push_back(ossia::camera_component_ptr(std::move(camPtr)));

    auto node = std::make_shared<ossia::scene_node>();
    node->id = m_id;
    node->children = std::move(children);

    auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
    roots->push_back(std::move(node));

    m_state->roots = std::move(roots);
    m_state->active_camera_id = m_id;
    m_version++;
    m_state->version = m_version;
    m_pending_dirty = ossia::scene_port::dirty_transform;
  }

  void operator()()
  {
    // Detect upstream camera-input pointer/version changes so a
    // scene_in that changed without a local control event still causes
    // a rebuild. Controls themselves trigger rebuild via their
    // update() callbacks.
    const ossia::scene_spec* cams[4]{
        &inputs.cam0.scene, &inputs.cam1.scene,
        &inputs.cam2.scene, &inputs.cam3.scene};
    bool upstream_changed = false;
    for(int i = 0; i < 4; ++i)
    {
      const auto* s = cams[i]->state.get();
      const int64_t v = s ? s->version : -1;
      if(m_cached_cam_state[i] != s || m_cached_cam_ver[i] != v)
      {
        upstream_changed = true;
        m_cached_cam_state[i] = s;
        m_cached_cam_ver[i] = v;
      }
    }

    if(inputs.mode.value == ins::CameraMode::Select)
    {
      // Forward the picked upstream scene directly — no local
      // shared_ptr identity to preserve beyond what upstream already
      // maintains.
      const int idx = inputs.index.value;
      const ossia::scene_spec* picked = nullptr;
      switch(idx)
      {
        case 0: picked = &inputs.cam0.scene; break;
        case 1: picked = &inputs.cam1.scene; break;
        case 2: picked = &inputs.cam2.scene; break;
        case 3: picked = &inputs.cam3.scene; break;
        default: picked = &inputs.cam0.scene; break;
      }
      outputs.scene_out.scene.state = picked->state;
      outputs.scene_out.dirty
          = (upstream_changed && picked->state) ? 0xFF : 0;
      m_pending_dirty = 0;
      return;
    }

    if(!m_state || upstream_changed)
      rebuild();
    outputs.scene_out.scene.state = m_state;
    outputs.scene_out.dirty = m_pending_dirty;
    m_pending_dirty = 0;
  }
};

}
