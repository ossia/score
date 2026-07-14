#pragma once

// Free functions that convert from the two source-data shapes
// HumanoidRetarget accepts (keypoint_stream from an ONNX PoseDetector,
// tracker_bundle_6 from a mocap / tracking-protocol device) into the
// canonical humanoid_pose. Separate header to keep HumanoidRetarget.hpp
// focused on orchestration + Offset-mode retargeting math.
//
// Both paths produce PARENT-LOCAL quaternions — that's the invariant
// HumanoidRetarget depends on. See the file comment at the top of
// HumanoidRetarget.hpp for why.

#include <Threedim/HumanoidPose.hpp>
#include <Threedim/HumanoidSourceMaps.hpp>

#include <array>
#include <cmath>

namespace Threedim
{

// ---------------------------------------------------------------------------
// Small quaternion helpers. Inline and header-only for zero TU overhead.
// (x, y, z, w) layout, matching ossia::skeleton_joint::rotation and
// humanoid_bone::q*.
// ---------------------------------------------------------------------------
inline void quat_mul_xyzw(
    const float a[4], const float b[4], float out[4]) noexcept
{
  const float x = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
  const float y = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
  const float z = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
  const float w = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
  out[0] = x; out[1] = y; out[2] = z; out[3] = w;
}

inline void quat_inv_xyzw(const float q[4], float out[4]) noexcept
{
  // Inverse of a unit quaternion = conjugate.
  out[0] = -q[0]; out[1] = -q[1]; out[2] = -q[2]; out[3] = q[3];
}

// Shortest-arc rotation from unit vector `from` to unit vector `to`.
// Produces the quaternion q such that q·from = to. Used to turn a
// canonical T-pose bone axis into the observed bone direction; this is
// inherently a 2-DoF answer (the twist around the bone's own length is
// undefined by just two direction endpoints). That's a hard limit of
// single-camera keypoint mocap; professional suits add IMU twist.
inline void shortest_arc(
    const float from[3], const float to[3], float out[4]) noexcept
{
  const float d = from[0] * to[0] + from[1] * to[1] + from[2] * to[2];
  const float eps = 1e-6f;

  if(d >= 1.f - eps)
  {
    // Aligned — identity.
    out[0] = 0.f; out[1] = 0.f; out[2] = 0.f; out[3] = 1.f;
    return;
  }
  if(d <= -1.f + eps)
  {
    // Antiparallel — 180° around ANY perpendicular axis. Pick one that
    // isn't (near-)parallel to `from` for numerical stability.
    float axis[3];
    if(std::fabs(from[0]) < 0.9f)
    {
      axis[0] = 1.f - from[0] * from[0];
      axis[1] = -from[0] * from[1];
      axis[2] = -from[0] * from[2];
    }
    else
    {
      axis[0] = -from[1] * from[0];
      axis[1] = 1.f - from[1] * from[1];
      axis[2] = -from[1] * from[2];
    }
    const float len
        = std::sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
    if(len > eps)
    {
      const float inv = 1.f / len;
      out[0] = axis[0] * inv;
      out[1] = axis[1] * inv;
      out[2] = axis[2] * inv;
    }
    else
    {
      out[0] = 1.f; out[1] = 0.f; out[2] = 0.f;
    }
    out[3] = 0.f;
    return;
  }

  // General case — half-vector formulation for numerical stability.
  const float cross[3] = {
      from[1] * to[2] - from[2] * to[1],
      from[2] * to[0] - from[0] * to[2],
      from[0] * to[1] - from[1] * to[0]};
  const float s = std::sqrt((1.f + d) * 2.f);
  const float invs = 1.f / s;
  out[0] = cross[0] * invs;
  out[1] = cross[1] * invs;
  out[2] = cross[2] * invs;
  out[3] = s * 0.5f;
}

// ---------------------------------------------------------------------------
// keypoints → humanoid_pose.
//
// Algorithm:
//   1. For each bone with a valid (parent_idx, child_idx) edge in the map
//      AND both keypoints' confidence ≥ threshold:
//        d_world[b] = normalize(kp[child] - kp[parent])
//        q_world[b] = shortestArc(kRestAxis[b], d_world[b])
//   2. Walk bones in topological order (enum order is already topological
//      because each bone's parent has a lower index). For each bone b:
//        - if no world rotation was computed, validity = 0
//        - if parent has no world rotation, emit q_world[b] as local
//          (root-relative behaviour — good fallback when upper chain data
//          is missing)
//        - else q_local[b] = inv(q_world[parent(b)]) * q_world[b]
//   3. Copy Hips world position from whatever landmark best represents it
//      (for BlazePose / COCO the hip midpoint; approximated as left_hip).
//      Used only by the root-motion toggle downstream.
// ---------------------------------------------------------------------------
inline humanoid_pose keypoints_to_humanoid_pose(
    const keypoint_stream& stream,
    const HumanoidKeypointMap& map,
    float confidence_threshold = 0.5f) noexcept
{
  humanoid_pose out{};

  // Step 1: per-bone world rotations.
  constexpr std::size_t N = std::size_t(humanoid_bone_index::Count);
  std::array<std::array<float, 4>, N> q_world{};
  std::array<bool, N> has_world{};

  const auto& kps = stream.keypoints;
  const int K = int(kps.size());

  for(std::size_t b = 0; b < N; ++b)
  {
    has_world[b] = false;
    q_world[b] = {0.f, 0.f, 0.f, 1.f};

    const auto& edge = map[b];
    if(!edge.valid() || edge.parent_idx == edge.child_idx)
      continue;
    if(edge.parent_idx >= K || edge.child_idx >= K)
      continue;

    const auto& p = kps[std::size_t(edge.parent_idx)];
    const auto& c = kps[std::size_t(edge.child_idx)];
    if(p.confidence < confidence_threshold
       || c.confidence < confidence_threshold)
      continue;

    float d[3] = {c.x - p.x, c.y - p.y, c.z - p.z};
    const float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if(len < 1e-6f)
      continue;
    const float inv = 1.f / len;
    d[0] *= inv; d[1] *= inv; d[2] *= inv;

    const auto& rest = kHumanoidRestAxis[b];
    shortest_arc(rest.data(), d, q_world[b].data());
    has_world[b] = true;
  }

  // Step 2: world → parent-local. Enum order is topological: each bone's
  // parent has a strictly lower index, so a single forward pass is safe.
  for(std::size_t b = 0; b < N; ++b)
  {
    auto& bone = out.bones[b];
    if(!has_world[b])
    {
      bone.validity = 0.f;
      bone.qx = 0.f; bone.qy = 0.f; bone.qz = 0.f; bone.qw = 1.f;
      continue;
    }

    const auto parent_idx = kHumanoidParent[b];
    if(parent_idx == humanoid_bone_index::Count
       || !has_world[std::size_t(parent_idx)])
    {
      // Root bone OR parent's world rotation is unknown — emit our world
      // rotation as local. For root this is correct; for a bone whose
      // parent failed to resolve this is a reasonable degradation (the
      // bone will orient absolutely rather than relative to a missing
      // parent, which at least keeps it visible).
      bone.qx = q_world[b][0];
      bone.qy = q_world[b][1];
      bone.qz = q_world[b][2];
      bone.qw = q_world[b][3];
    }
    else
    {
      float inv_parent[4];
      quat_inv_xyzw(q_world[std::size_t(parent_idx)].data(), inv_parent);
      float local[4];
      quat_mul_xyzw(inv_parent, q_world[b].data(), local);
      bone.qx = local[0]; bone.qy = local[1];
      bone.qz = local[2]; bone.qw = local[3];
    }
    bone.validity = 1.f;
  }

  // Hip translation — grab the parent keypoint of the Spine edge as the
  // best "pelvis" proxy (BlazePose landmark 23 = left_hip, COCO 11 =
  // left_hip). Not the true midpoint, but close enough for single-camera
  // root motion; users who need precision should use a tracker workflow.
  const auto& spine_edge = map[std::size_t(humanoid_bone_index::Spine)];
  if(spine_edge.parent_idx >= 0 && spine_edge.parent_idx < K)
  {
    const auto& hip_kp = kps[std::size_t(spine_edge.parent_idx)];
    if(hip_kp.confidence >= confidence_threshold)
    {
      out.hip_x = hip_kp.x;
      out.hip_y = hip_kp.y;
      out.hip_z = hip_kp.z;
    }
  }

  return out;
}

// ---------------------------------------------------------------------------
// trackers → humanoid_pose.
//
// With only 6 trackers (head, hips, 2 hands, 2 feet) we directly drive
// those 6 bones and leave the intermediate bones (spine, shoulders,
// elbows, knees) at their retarget rest. Getting those bones to follow
// realistically needs either more trackers (10-point Vive Full-Body) or
// a downstream 2-bone IK chain (InverseKinematics process) keyed on
// shoulder + wrist tracker positions as (root, target). v1 keeps the
// retargeter unopinionated — we fill what we're given.
//
// Tracker quaternions are world-space by convention (PSN, OSC, VRPN all
// report world transforms). Parent-local is produced by inverting the
// parent bone's tracker rotation if that parent also has a tracker;
// otherwise the bone inherits the world rotation directly.
// ---------------------------------------------------------------------------
inline humanoid_pose trackers_to_humanoid_pose(
    const tracker_bundle_6& t) noexcept
{
  humanoid_pose out{};

  // Slot 1:1 mapping — which canonical bone gets which tracker.
  struct Slot
  {
    humanoid_bone_index bone;
    const tracker_pose* tr;
  };
  const Slot slots[] = {
      {humanoid_bone_index::Hips, &t.hips},
      {humanoid_bone_index::Head, &t.head},
      {humanoid_bone_index::LeftHand, &t.left_hand},
      {humanoid_bone_index::RightHand, &t.right_hand},
      {humanoid_bone_index::LeftFoot, &t.left_foot},
      {humanoid_bone_index::RightFoot, &t.right_foot},
  };

  // Gather world rotations.
  constexpr std::size_t N = std::size_t(humanoid_bone_index::Count);
  std::array<std::array<float, 4>, N> q_world{};
  std::array<bool, N> has_world{};
  for(std::size_t b = 0; b < N; ++b)
  {
    q_world[b] = {0.f, 0.f, 0.f, 1.f};
    has_world[b] = false;
  }

  for(const auto& slot : slots)
  {
    if(slot.tr->validity < 0.5f)
      continue;
    const std::size_t idx = std::size_t(slot.bone);
    q_world[idx] = {slot.tr->qx, slot.tr->qy, slot.tr->qz, slot.tr->qw};
    has_world[idx] = true;
  }

  // World → parent-local, same pattern as the keypoint path. Bones whose
  // parent has no tracker fall through to "emit world as local", which
  // makes them pose relative to the world origin — correct for Head /
  // Hands when their parent chain (Neck, LowerArm) isn't tracker-driven.
  for(std::size_t b = 0; b < N; ++b)
  {
    auto& bone = out.bones[b];
    if(!has_world[b])
    {
      bone.validity = 0.f;
      continue;
    }

    const auto parent_idx = kHumanoidParent[b];
    if(parent_idx == humanoid_bone_index::Count
       || !has_world[std::size_t(parent_idx)])
    {
      bone.qx = q_world[b][0]; bone.qy = q_world[b][1];
      bone.qz = q_world[b][2]; bone.qw = q_world[b][3];
    }
    else
    {
      float inv_parent[4];
      quat_inv_xyzw(q_world[std::size_t(parent_idx)].data(), inv_parent);
      float local[4];
      quat_mul_xyzw(inv_parent, q_world[b].data(), local);
      bone.qx = local[0]; bone.qy = local[1];
      bone.qz = local[2]; bone.qw = local[3];
    }
    bone.validity = 1.f;
  }

  // Hip position = hips tracker position (if tracking).
  if(t.hips.validity >= 0.5f)
  {
    out.hip_x = t.hips.x;
    out.hip_y = t.hips.y;
    out.hip_z = t.hips.z;
  }

  return out;
}

} // namespace Threedim
