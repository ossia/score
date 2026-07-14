#pragma once

// humanoid_pose: canonical intermediate for live mocap -> rigged-character
// retargeting. One fixed set of 22 bones that every source adapter populates and
// HumanoidRetarget consumes.
//
// Flows through halp's structured-value port machinery, matching the existing
// DetectedPose pattern; no new libossia port type.
//
// Rotations are local parent-relative quaternions (x, y, z, w); adapters convert
// their source's native representation into that shape. `validity` is a 0..1
// per-bone confidence -- BlazePose visibility, tracker occlusion -- that
// downstream uses to freeze a bone rather than trust it. `hip_position` is the
// only world-space translation that flows through, since every other bone's
// position is implied by the target rig's proportions, and it is used only with
// HumanoidRetarget's root-motion toggle on.

#include <halp/controls.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace Threedim
{

// Canonical bone set. Indexed access via the enum; iterate with
// humanoid_bone_index::Count. Order is stable — adapters and retargeter
// presets both depend on it.
enum class humanoid_bone_index : uint8_t
{
  Hips = 0,
  Spine,
  Chest,
  Neck,
  Head,

  LeftShoulder,
  LeftUpperArm,
  LeftLowerArm,
  LeftHand,

  RightShoulder,
  RightUpperArm,
  RightLowerArm,
  RightHand,

  LeftUpperLeg,
  LeftLowerLeg,
  LeftFoot,
  LeftToes,

  RightUpperLeg,
  RightLowerLeg,
  RightFoot,
  RightToes,

  Count
};

// Per-bone pose. 20-byte halp-structured record (5 floats).
struct humanoid_bone
{
  // Parent-relative rotation quaternion, (x, y, z, w). Identity = {0,0,0,1}.
  float qx{0.f};
  float qy{0.f};
  float qz{0.f};
  float qw{1.f};

  // 0..1 confidence. 0 means "no reliable data for this bone, retargeter
  // should ignore this frame for this bone". 1 = fully trusted.
  float validity{1.f};

  halp_field_names(qx, qy, qz, qw, validity);
};

// Fixed-size bone array — std::array plays nicely with halp serialization
// (same way DetectedPose uses std::vector, except the size is known and
// we can index by enum without a lookup).
struct humanoid_pose
{
  std::array<humanoid_bone, std::size_t(humanoid_bone_index::Count)> bones{};

  // World-space translation of the hip (Hips) root. Only consumed when
  // root-motion is enabled on HumanoidRetarget; otherwise ignored.
  float hip_x{0.f};
  float hip_y{0.f};
  float hip_z{0.f};

  // Wall-clock frame counter. Increments on every adapter emit. Used by
  // consumers for dirty tracking (skip work when version hasn't advanced).
  int64_t version{0};

  // Convenience: access a bone by enum.
  humanoid_bone& operator[](humanoid_bone_index b) noexcept
  {
    return bones[std::size_t(b)];
  }
  const humanoid_bone& operator[](humanoid_bone_index b) const noexcept
  {
    return bones[std::size_t(b)];
  }

  halp_field_names(bones, hip_x, hip_y, hip_z, version);
};

// =============================================================================
// Keypoint ingestion type — structurally compatible with the DetectedPose
// struct from score-addon-onnx (same field names, same layout) so halp's
// field-name-based port marshalling can carry a DetectedPose through a
// port typed as keypoint_stream without cross-addon header dependency.
//
// Kept in Threedim deliberately: HumanoidRetarget consumes it, but we
// don't want score-plugin-threedim to link against score-addon-onnx.
// =============================================================================
struct keypoint_3d
{
  float x{0.f};
  float y{0.f};
  float z{0.f};
  float confidence{0.f};

  halp_field_names(x, y, z, confidence);
};

struct keypoint_stream
{
  std::vector<keypoint_3d> keypoints;
  float mean_confidence{0.f};

  halp_field_names(keypoints, mean_confidence);
};

// =============================================================================
// Tracker bundle: 6 slots matching a common VR / optical-mocap full-body layout
// (head, hips, 2 hands, 2 feet). Each carries a world-space position, a
// world-space quaternion, and a validity, so lost tracking skips instead of
// slamming the character to the origin.
//
// Richer layouts (10-point Vive Full-Body, Xsens 17-IMU, OptiTrack marker sets)
// would be additional bundle_N struct types; users with those rigs can drive
// these 6 slots from the subset they trust.
// =============================================================================
struct tracker_pose
{
  // World-space translation.
  float x{0.f};
  float y{0.f};
  float z{0.f};

  // World-space quaternion (x, y, z, w). Identity = {0, 0, 0, 1}.
  float qx{0.f};
  float qy{0.f};
  float qz{0.f};
  float qw{1.f};

  // 0..1 tracking confidence. 0 = "tracker offline, ignore this frame".
  float validity{0.f};

  halp_field_names(x, y, z, qx, qy, qz, qw, validity);
};

struct tracker_bundle_6
{
  tracker_pose head;
  tracker_pose hips;
  tracker_pose left_hand;
  tracker_pose right_hand;
  tracker_pose left_foot;
  tracker_pose right_foot;

  halp_field_names(head, hips, left_hand, right_hand, left_foot, right_foot);
};

} // namespace Threedim
