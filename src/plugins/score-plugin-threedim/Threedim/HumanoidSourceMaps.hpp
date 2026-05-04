#pragma once

// Source-side tables used by HumanoidRetarget's conversion step:
//
//   - per-workflow keypoint→bone mapping (BlazePose 33, COCO-17, RTMPose
//     Whole 133) — each entry says "bone B's direction is landmark parent_idx
//     to child_idx in this workflow"
//   - canonical T-pose bone axes — the world-space direction each bone
//     points in the canonical T-pose (e.g. LeftUpperArm is -X). Used as
//     the "rest direction" of each bone for the shortest-arc computation.
//   - bone hierarchy (parent-of-bone) — needed to convert world rotations
//     to parent-relative quaternions after the shortest-arc pass.
//
// Kept separate from HumanoidRetarget.hpp to keep the retargeter file
// focused on orchestration + math.

#include <Threedim/HumanoidPose.hpp>

#include <array>
#include <cstdint>

namespace Threedim
{

// ---------------------------------------------------------------------------
// Bone tree: for each canonical bone, its parent bone (or Count if root).
// Matches the humanoid_bone_index enum order.
// ---------------------------------------------------------------------------
inline constexpr std::array<
    humanoid_bone_index,
    std::size_t(humanoid_bone_index::Count)>
    kHumanoidParent = {{
        humanoid_bone_index::Count,            // Hips (root)
        humanoid_bone_index::Hips,             // Spine
        humanoid_bone_index::Spine,            // Chest
        humanoid_bone_index::Chest,            // Neck
        humanoid_bone_index::Neck,             // Head

        humanoid_bone_index::Chest,            // LeftShoulder
        humanoid_bone_index::LeftShoulder,     // LeftUpperArm
        humanoid_bone_index::LeftUpperArm,     // LeftLowerArm
        humanoid_bone_index::LeftLowerArm,     // LeftHand

        humanoid_bone_index::Chest,            // RightShoulder
        humanoid_bone_index::RightShoulder,    // RightUpperArm
        humanoid_bone_index::RightUpperArm,    // RightLowerArm
        humanoid_bone_index::RightLowerArm,    // RightHand

        humanoid_bone_index::Hips,             // LeftUpperLeg
        humanoid_bone_index::LeftUpperLeg,     // LeftLowerLeg
        humanoid_bone_index::LeftLowerLeg,     // LeftFoot
        humanoid_bone_index::LeftFoot,         // LeftToes

        humanoid_bone_index::Hips,             // RightUpperLeg
        humanoid_bone_index::RightUpperLeg,    // RightLowerLeg
        humanoid_bone_index::RightLowerLeg,    // RightFoot
        humanoid_bone_index::RightFoot,        // RightToes
    }};

// ---------------------------------------------------------------------------
// Canonical T-pose bone axes. Y-up, right-handed, model facing +Z.
//
// Each entry is the world-space unit direction the bone's parent→child
// segment points in the canonical T-pose. The retargeter uses these as
// the "from" vector in the shortest-arc rotation that aligns the bone
// with the current landmark-derived direction.
//
// Conventions:
//   - Spine / Neck / Head chain points up (+Y)
//   - Arms point outward (-X for left, +X for right) along the horizontal
//   - Legs point down (-Y)
//   - Toes point forward (+Z)
//   - Shoulders are small bones from spine to upper-arm root; treat as
//     pointing toward the upper-arm (horizontal left/right)
//   - Hips bone itself is the root; no direction (identity).
// ---------------------------------------------------------------------------
inline constexpr std::array<
    std::array<float, 3>,
    std::size_t(humanoid_bone_index::Count)>
    kHumanoidRestAxis = {{
        {0.f, 0.f, 0.f},   // Hips — root, no direction
        {0.f, 1.f, 0.f},   // Spine +Y
        {0.f, 1.f, 0.f},   // Chest +Y
        {0.f, 1.f, 0.f},   // Neck +Y
        {0.f, 1.f, 0.f},   // Head +Y

        {-1.f, 0.f, 0.f},  // LeftShoulder -X
        {-1.f, 0.f, 0.f},  // LeftUpperArm -X
        {-1.f, 0.f, 0.f},  // LeftLowerArm -X
        {-1.f, 0.f, 0.f},  // LeftHand -X

        {1.f, 0.f, 0.f},   // RightShoulder +X
        {1.f, 0.f, 0.f},   // RightUpperArm +X
        {1.f, 0.f, 0.f},   // RightLowerArm +X
        {1.f, 0.f, 0.f},   // RightHand +X

        {0.f, -1.f, 0.f},  // LeftUpperLeg -Y
        {0.f, -1.f, 0.f},  // LeftLowerLeg -Y
        {0.f, -1.f, 0.f},  // LeftFoot -Y
        {0.f, 0.f, 1.f},   // LeftToes +Z

        {0.f, -1.f, 0.f},  // RightUpperLeg -Y
        {0.f, -1.f, 0.f},  // RightLowerLeg -Y
        {0.f, -1.f, 0.f},  // RightFoot -Y
        {0.f, 0.f, 1.f},   // RightToes +Z
    }};

// ---------------------------------------------------------------------------
// Keypoint mapping: for each canonical bone, (parent_keypoint_idx,
// child_keypoint_idx) into the workflow's keypoint array. -1 means this
// bone isn't derivable from this workflow (the adapter will skip it,
// keeping the target bone at its rest rotation).
// ---------------------------------------------------------------------------
struct HumanoidKeypointEdge
{
  int16_t parent_idx{-1};
  int16_t child_idx{-1};
  bool valid() const noexcept { return parent_idx >= 0 && child_idx >= 0; }
};

using HumanoidKeypointMap = std::array<
    HumanoidKeypointEdge,
    std::size_t(humanoid_bone_index::Count)>;

// ---------------------------------------------------------------------------
// BlazePose (33 landmarks).
// Index reference:
//   0: nose, 1: left_eye_inner, 2: left_eye, 3: left_eye_outer,
//   4: right_eye_inner, 5: right_eye, 6: right_eye_outer,
//   7: left_ear, 8: right_ear,
//   9: mouth_left, 10: mouth_right,
//   11: left_shoulder, 12: right_shoulder,
//   13: left_elbow, 14: right_elbow,
//   15: left_wrist, 16: right_wrist,
//   17..22: left/right pinky/index/thumb (hand subdetail)
//   23: left_hip, 24: right_hip,
//   25: left_knee, 26: right_knee,
//   27: left_ankle, 28: right_ankle,
//   29: left_heel, 30: right_heel,
//   31: left_foot_index, 32: right_foot_index
//
// Bone directions are parent_kp → child_kp:
//   - Spine: midpoint(hips) → midpoint(shoulders). Approximated as
//     left_hip → left_shoulder (an acceptable approximation for a
//     single-segment spine; precise midpoint handling would need
//     a helper with synthesized virtual landmarks).
//   - Chest / Neck approximated similarly.
//   - Shoulders (the bone from spine to upper-arm root) are treated as
//     midpoint(shoulders) → shoulder. Again approximated directly.
//   - Toes: ankle → foot_index
// ---------------------------------------------------------------------------
inline constexpr HumanoidKeypointMap kBlazePoseMap = {{
    {-1, -1},              // Hips (root)
    {23, 11},              // Spine: left_hip → left_shoulder
    {11, 12},              // Chest: shoulders pair  (approximation)
    {11, 0},               // Neck:  left_shoulder → nose (approx)
    {0, 2},                // Head:  nose → left_eye (approx)

    {11, 11},              // LeftShoulder (collar): degenerate — map skipped by validity
    {11, 13},              // LeftUpperArm: left_shoulder → left_elbow
    {13, 15},              // LeftLowerArm: left_elbow → left_wrist
    {15, 19},              // LeftHand:     left_wrist → left_index

    {12, 12},              // RightShoulder (collar): skipped
    {12, 14},              // RightUpperArm
    {14, 16},              // RightLowerArm
    {16, 20},              // RightHand

    {23, 25},              // LeftUpperLeg
    {25, 27},              // LeftLowerLeg
    {27, 29},              // LeftFoot
    {27, 31},              // LeftToes:     ankle → foot_index

    {24, 26},              // RightUpperLeg
    {26, 28},              // RightLowerLeg
    {28, 30},              // RightFoot
    {28, 32},              // RightToes
}};

// ---------------------------------------------------------------------------
// COCO-17 layout (YOLO-pose, ViTPose, RTMPose_COCO).
// Index reference:
//   0: nose, 1: left_eye, 2: right_eye, 3: left_ear, 4: right_ear,
//   5: left_shoulder, 6: right_shoulder,
//   7: left_elbow, 8: right_elbow,
//   9: left_wrist, 10: right_wrist,
//   11: left_hip, 12: right_hip,
//   13: left_knee, 14: right_knee,
//   15: left_ankle, 16: right_ankle
//
// No toes / feet detail, no fingers — those bones are flagged as
// unmappable and will keep their target rest rotation.
// ---------------------------------------------------------------------------
inline constexpr HumanoidKeypointMap kCoco17Map = {{
    {-1, -1},              // Hips
    {11, 5},               // Spine:  left_hip → left_shoulder (approx)
    {5, 6},                // Chest:  shoulders (approx)
    {5, 0},                // Neck:   shoulder → nose (approx)
    {0, 1},                // Head:   nose → left_eye

    {-1, -1},              // LeftShoulder — no dedicated landmark
    {5, 7},                // LeftUpperArm
    {7, 9},                // LeftLowerArm
    {-1, -1},              // LeftHand — no wrist-to-hand direction in COCO

    {-1, -1},              // RightShoulder
    {6, 8},                // RightUpperArm
    {8, 10},               // RightLowerArm
    {-1, -1},              // RightHand

    {11, 13},              // LeftUpperLeg
    {13, 15},              // LeftLowerLeg
    {-1, -1},              // LeftFoot — ankle only
    {-1, -1},              // LeftToes

    {12, 14},              // RightUpperLeg
    {14, 16},              // RightLowerLeg
    {-1, -1},              // RightFoot
    {-1, -1},              // RightToes
}};

// ---------------------------------------------------------------------------
// RTMPose Whole-body 133 keypoints — first 17 match COCO, 17..22 face,
// 23..90 face mesh, 91..132 hands. For body retargeting we reuse the
// first 17 (same as COCO), and optionally pull finger landmarks for a
// richer hand (Hand bone direction = wrist → middle_finger_mcp).
//
// v1: use only the COCO subset. Hands would require a 21-landmark map
// (follow-up).
// ---------------------------------------------------------------------------
inline constexpr HumanoidKeypointMap kRTMPoseWholeMap = kCoco17Map;

} // namespace Threedim
