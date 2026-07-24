#pragma once

// Threedim::HumanoidRetarget — live scene filter that drives a rigged
// model's skeleton from a humanoid_pose stream.
//
// Inputs:
//   - scene_in: an ossia::scene_spec carrying a rigged asset (at least one
//     skeleton_component). Typically comes from Threedim::AssetLoader.
//   - pose_in:  std::optional<humanoid_pose> from a source adapter (e.g.
//     PoseKeypointsToHumanoid wrapped around an ONNX PoseDetector, or
//     TrackedBonesToHumanoid over PSN/RTTrP trackers).
//
// Controls:
//   - Target rig preset: Mixamo / VRM / Unreal Mannequin bone-name
//     convention. Selects which joint names we look up against the
//     scene's skeleton_component.
//   - Capture rest pose (impulse): snapshot both sides' current state as
//     the retarget reference. Required before any motion transfers.
//   - Root motion (toggle) + Root scale: optional Hips translation
//     driven by the source's hip_position delta from rest.
//
// Output:
//   - scene_out: the incoming scene_spec with ONLY the mapped joints'
//     rotations (and optionally Hips translation) replaced. Every other
//     joint, every mesh, every material, the scene hierarchy, version
//     counters on other state — all passed through unchanged.
//
// Math (Offset / delta-from-rest mode, the default and correct choice
// when source and target rigs have different axis conventions):
//
//     q_tgt_cur = q_tgt_rest * ( inverse(q_src_rest) * q_src_cur )
//
// Calibration (both sides at once) captures q_src_rest per canonical
// bone and q_tgt_rest per resolved target joint. The delta is then a
// parent-relative quaternion that transfers cleanly even if the source
// is, say, a BlazePose landmark graph and the target is a Mixamo FBX —
// as long as the adapter produces parent-relative rotations, the math
// works. Per-bone axis correction matrices are a follow-up (needed for
// some exotic rigs; not a v1 concern).
//
// No smoothing here — smoothing belongs in the adapter, pre-pose_spec.
// No IK here — chain `InverseKinematics` after this process for
// hand/foot-prop reach; the two compose cleanly on scene_spec.

#include <Threedim/HumanoidPose.hpp>
#include <Threedim/HumanoidPresets.hpp>
#include <Threedim/HumanoidSourceAdapters.hpp>
#include <Threedim/HumanoidSourceMaps.hpp>

#include <halp/controls.hpp>
#include <halp/controls.buttons.hpp>
#include <halp/meta.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>

namespace Threedim
{

// Picks which input shape the retargeter consumes this frame. The
// matching input ports are always present on the process (halp doesn't
// hide ports conditionally); the combobox just tells the dispatch which
// one to translate into humanoid_pose.
enum class HumanoidSourceType : uint8_t
{
  Off = 0,         // Passthrough (no motion applied)
  BlazePose,       // keypoints_in, BlazePose 33-landmark ordering
  Coco17,          // keypoints_in, COCO-17 (YOLO-pose / ViTPose / RTMPose_COCO)
  RTMPoseWhole,    // keypoints_in, RTMPose_Whole (body subset of 133)
  Trackers6,       // trackers_in, 6 DOF (head / hips / 2 hands / 2 feet)
  Count
};

class HumanoidRetarget
{
public:
  halp_meta(name, "Humanoid Retarget")
  halp_meta(category, "Visuals/3D/Scene")
  halp_meta(c_name, "humanoid_retarget")
  halp_meta(authors, "ossia team")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/humanoid-retarget.html")
  halp_meta(uuid, "7e1f4d8a-2c6b-4e7f-9a35-6c4b8d2e0f1a")

  struct ins
  {
    struct
    {
      halp_meta(name, "Scene In");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene_in;

    // Keypoint input — populated when Source is a PoseDetector workflow.
    // Structurally compatible with score-addon-onnx's DetectedPose
    // (matching halp_field_names), so a DetectedPose port wires directly.
    struct
    {
      halp_meta(name, "Keypoints");
      std::optional<keypoint_stream> value;
    } keypoints_in;

    // Tracker input — populated when Source is Trackers6. The user wires
    // OSC-emitted xyz+quat streams from a PSN/RTTrP/VRPN device into the
    // matching tracker_pose slots of the bundle.
    struct
    {
      halp_meta(name, "Trackers");
      std::optional<tracker_bundle_6> value;
    } trackers_in;

    struct : halp::combobox_t<"Source", HumanoidSourceType>
    {
      struct range
      {
        std::string_view values[5]{
            "Off", "BlazePose", "COCO-17", "RTMPose Whole", "6DOF Trackers"};
        int init{0};
      };
      void update(HumanoidRetarget& self)
      {
        // Source-shape change invalidates the captured source rest pose;
        // the map of landmark→bone (and bone→tracker) differs, so previous
        // "rest" values aren't meaningful under the new source.
        self.m_calibrated = false;
      }
    } source;

    struct : halp::hslider_f32<"Confidence", halp::range{0.f, 1.f, 0.5f}>
    {
      halp_meta(description, "Per-keypoint confidence threshold");
    } confidence_threshold;

    struct : halp::combobox_t<"Target rig", HumanoidRigPreset>
    {
      struct range
      {
        std::string_view values[3]{"Mixamo", "VRM", "Unreal Mannequin"};
        int init{0};
      };
      void update(HumanoidRetarget& self)
      {
        // Bone-name table change invalidates the cached joint index
        // lookups and the captured target rest pose; force a fresh
        // calibration on the next frame that has both inputs.
        self.m_calibrated = false;
      }
    } preset;

    halp::toggle<"Root motion"> root_motion;

    struct : halp::hslider_f32<"Root scale", halp::range{0.01f, 10.f, 1.f}>
    {
    } root_scale;

    struct : halp::impulse_button<"Capture rest pose">
    {
      void update(HumanoidRetarget& self) { self.m_need_calibrate = true; }
    } calibrate;
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

  void operator()()
  {
    const auto& in = inputs.scene_in.scene;
    if(!in.state || !in.state->roots)
    {
      outputs.scene_out.scene.state.reset();
      outputs.scene_out.dirty = 0;
      return;
    }

    // Translate the selected source into a humanoid_pose. Off mode and
    // "source has no fresh data" both fall through to a clean passthrough
    // so downstream nodes see the input unchanged until motion starts.
    std::optional<humanoid_pose> maybe_pose
        = composeSourcePose(inputs.confidence_threshold.value);
    if(!maybe_pose)
    {
      outputs.scene_out.scene = in;
      outputs.scene_out.dirty = 0;
      return;
    }

    const auto& pose = *maybe_pose;

    // Resolve the skeleton — first entry in the scene's skeletons list.
    // Multi-skeleton assets (per-skin glTF) are a follow-up: v1 retargets
    // the first one, which covers 100% of Mixamo / VRM / single-rig
    // scenes.
    if(!in.state->skeletons || in.state->skeletons->empty())
    {
      outputs.scene_out.scene = in;
      outputs.scene_out.dirty = 0;
      return;
    }
    const auto& srcSkel = *(*in.state->skeletons)[0];
    if(srcSkel.joints.empty())
    {
      outputs.scene_out.scene = in;
      outputs.scene_out.dirty = 0;
      return;
    }

    // Calibrate on demand. Two triggers:
    //   - user pressed "Capture rest pose"
    //   - preset combobox changed (invalidates previous joint lookups)
    if(m_need_calibrate || !m_calibrated)
    {
      calibrate(srcSkel, pose);
      m_need_calibrate = false;
    }

    // Clone the skeleton so other consumers of the input scene don't see
    // our mutations. This is the same pattern InverseKinematics uses.
    auto newSkel = std::make_shared<ossia::skeleton_component>(srcSkel);

    // Per-bone offset-mode retarget:
    //   q_tgt_new = q_tgt_rest * ( inverse(q_src_rest) * q_src_cur )
    for(std::size_t b = 0; b < std::size_t(humanoid_bone_index::Count); ++b)
    {
      const int32_t tgt = m_target_joint_indices[b];
      if(tgt < 0 || tgt >= int32_t(newSkel->joints.size()))
        continue;

      const auto& src_cur = pose.bones[b];
      if(src_cur.validity < kValidityThreshold)
        continue; // trust the target's current rotation (kept from clone)

      const float src_cur_q[4] = {
          src_cur.qx, src_cur.qy, src_cur.qz, src_cur.qw};
      float inv_src_rest[4];
      quat_inv(m_source_rest[b], inv_src_rest);

      float delta[4];
      quat_mul(inv_src_rest, src_cur_q, delta);

      float out[4];
      quat_mul(m_target_rest[b], delta, out);

      auto& tgtJoint = newSkel->joints[tgt];
      tgtJoint.rotation[0] = out[0];
      tgtJoint.rotation[1] = out[1];
      tgtJoint.rotation[2] = out[2];
      tgtJoint.rotation[3] = out[3];
    }

    // Root motion — apply source hip delta to target hip translation,
    // scaled by the user control. Off by default (most live scenes want
    // animate-in-place; locomotion is a deliberate choice).
    if(inputs.root_motion.value)
    {
      const int32_t hipsIdx
          = m_target_joint_indices[std::size_t(humanoid_bone_index::Hips)];
      if(hipsIdx >= 0 && hipsIdx < int32_t(newSkel->joints.size()))
      {
        const float s = inputs.root_scale.value;
        auto& hip = newSkel->joints[hipsIdx];
        hip.translation[0]
            = m_target_rest_hip_tr[0] + (pose.hip_x - m_source_rest_hip[0]) * s;
        hip.translation[1]
            = m_target_rest_hip_tr[1] + (pose.hip_y - m_source_rest_hip[1]) * s;
        hip.translation[2]
            = m_target_rest_hip_tr[2] + (pose.hip_z - m_source_rest_hip[2]) * s;
      }
    }

    newSkel->dirty_index++;

    // Emit a fresh scene_state that shares everything with the input
    // except the skeletons vector.
    auto state = std::make_shared<ossia::scene_state>(*in.state);
    auto skels
        = std::make_shared<std::vector<ossia::skeleton_component_ptr>>();
    skels->reserve(in.state->skeletons->size());
    for(std::size_t i = 0; i < in.state->skeletons->size(); ++i)
      skels->push_back(
          i == 0 ? ossia::skeleton_component_ptr(newSkel)
                 : (*in.state->skeletons)[i]);
    state->skeletons = std::move(skels);
    state->version = ++m_version_counter;
    state->dirty_index = in.state->dirty_index + 1;

    m_state = std::move(state);
    outputs.scene_out.scene.state = m_state;
    outputs.scene_out.dirty = ossia::scene_port::dirty_transform;
  }

private:
  // Rotation confidence below which we don't override the target bone.
  // Adapters default bone validity to 1.0; BlazePose maps landmark
  // visibility into [0, 1]. 0.5 is a reasonable "believe this" line.
  static constexpr float kValidityThreshold = 0.5f;

  // Hamilton quaternion multiply. (x, y, z, w) ordering.
  static void quat_mul(const float a[4], const float b[4], float out[4]) noexcept
  {
    const float x = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
    const float y = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
    const float z = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
    const float w = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
    out[0] = x;
    out[1] = y;
    out[2] = z;
    out[3] = w;
  }

  // Inverse of a unit quaternion = conjugate. Adapters should be
  // emitting normalized rotations; if they drift, the math still
  // produces a stable result but scale factors creep in.
  static void quat_inv(const float q[4], float out[4]) noexcept
  {
    out[0] = -q[0];
    out[1] = -q[1];
    out[2] = -q[2];
    out[3] = q[3];
  }

  void calibrate(
      const ossia::skeleton_component& skel,
      const humanoid_pose& pose) noexcept
  {
    const auto& map = humanoidBoneMap(inputs.preset.value);

    for(std::size_t b = 0; b < std::size_t(humanoid_bone_index::Count); ++b)
    {
      // Snapshot source rest pose quaternion (identity-ish if adapter
      // hasn't moved yet; whatever is there is what "neutral" means
      // for this capture).
      m_source_rest[b][0] = pose.bones[b].qx;
      m_source_rest[b][1] = pose.bones[b].qy;
      m_source_rest[b][2] = pose.bones[b].qz;
      m_source_rest[b][3] = pose.bones[b].qw;

      m_target_joint_indices[b] = -1;
      if(map[b].empty())
        continue; // preset intentionally skips this bone (e.g. UpperChest)

      const int32_t idx = skel.find_joint(map[b]);
      if(idx < 0)
        continue;
      m_target_joint_indices[b] = idx;

      // Snapshot target rest rotation.
      const auto& j = skel.joints[std::size_t(idx)];
      m_target_rest[b][0] = j.rotation[0];
      m_target_rest[b][1] = j.rotation[1];
      m_target_rest[b][2] = j.rotation[2];
      m_target_rest[b][3] = j.rotation[3];

      if(b == std::size_t(humanoid_bone_index::Hips))
      {
        m_target_rest_hip_tr[0] = j.translation[0];
        m_target_rest_hip_tr[1] = j.translation[1];
        m_target_rest_hip_tr[2] = j.translation[2];
      }
    }

    m_source_rest_hip[0] = pose.hip_x;
    m_source_rest_hip[1] = pose.hip_y;
    m_source_rest_hip[2] = pose.hip_z;

    m_calibrated = true;
  }

  // Dispatch the selected source toggle into a humanoid_pose. Returns
  // nullopt when the source is Off or no fresh data is present — in that
  // case operator() passes the input scene through unchanged.
  std::optional<humanoid_pose>
  composeSourcePose(float confidence_threshold) noexcept
  {
    const auto src = inputs.source.value;
    switch(src)
    {
      case HumanoidSourceType::Off:
      case HumanoidSourceType::Count:
        return std::nullopt;

      case HumanoidSourceType::BlazePose:
        if(!inputs.keypoints_in.value
           || inputs.keypoints_in.value->keypoints.empty())
          return std::nullopt;
        return keypoints_to_humanoid_pose(
            *inputs.keypoints_in.value, kBlazePoseMap, confidence_threshold);

      case HumanoidSourceType::Coco17:
        if(!inputs.keypoints_in.value
           || inputs.keypoints_in.value->keypoints.empty())
          return std::nullopt;
        return keypoints_to_humanoid_pose(
            *inputs.keypoints_in.value, kCoco17Map, confidence_threshold);

      case HumanoidSourceType::RTMPoseWhole:
        if(!inputs.keypoints_in.value
           || inputs.keypoints_in.value->keypoints.empty())
          return std::nullopt;
        return keypoints_to_humanoid_pose(
            *inputs.keypoints_in.value, kRTMPoseWholeMap,
            confidence_threshold);

      case HumanoidSourceType::Trackers6:
        if(!inputs.trackers_in.value)
          return std::nullopt;
        return trackers_to_humanoid_pose(*inputs.trackers_in.value);
    }
    return std::nullopt;
  }

public:
  // Persisted across score-document saves (serialized with process state).
  bool m_calibrated{false};
  std::array<float[4], std::size_t(humanoid_bone_index::Count)> m_source_rest{};
  std::array<float[4], std::size_t(humanoid_bone_index::Count)> m_target_rest{};
  std::array<int32_t, std::size_t(humanoid_bone_index::Count)>
      m_target_joint_indices{};
  float m_target_rest_hip_tr[3]{0.f, 0.f, 0.f};
  float m_source_rest_hip[3]{0.f, 0.f, 0.f};

  // Ephemeral.
  bool m_need_calibrate{false};
  std::shared_ptr<ossia::scene_state> m_state;
  int64_t m_version_counter{0};
};

} // namespace Threedim
