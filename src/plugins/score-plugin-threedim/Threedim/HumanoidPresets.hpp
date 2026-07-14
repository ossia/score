#pragma once

// Target rig presets for HumanoidRetarget: compile-time tables mapping
// canonical humanoid_bone_index → the target skeleton's joint name for
// three common conventions:
//
//   - Mixamo (mixamorig:*) — ubiquitous for indie / live / education
//   - VRM — VTubing standard; Ready Player Me derivatives all use this
//     spec's bone names (the VRM humanoid bone list)
//   - Unreal Mannequin — game-dev convention; also matches the
//     output of many BVH-to-FBX converters and most "clean" glTF rigs
//
// Tables are std::array<std::string_view>, compile-time, zero-overhead.
// If an entry is empty the target rig doesn't have a corresponding bone
// and HumanoidRetarget will silently skip it (e.g. Mixamo has no explicit
// Toes bone so LeftToes / RightToes are empty).
//
// Not user-editable by design (see project_decisions.md): if a rig doesn't
// fit these three presets, add a fourth preset in code rather than
// exposing a JSON text-input escape hatch that tends to silently half-work.

#include <Threedim/HumanoidPose.hpp>

#include <array>
#include <string_view>

namespace Threedim
{

using HumanoidBoneMap = std::array<
    std::string_view,
    std::size_t(humanoid_bone_index::Count)>;

enum class HumanoidRigPreset : uint8_t
{
  Mixamo = 0,
  VRM,
  UnrealMannequin,
  Count
};

// Mixamo — "mixamorig:" prefix, title-cased component names.
// Spine / Spine1 / Spine2 are three bones; we map the canonical
// Spine→Spine, Chest→Spine1, (no UpperChest) and Neck/Head directly.
// Mixamo has no explicit Toes bones; we map to *ToeBase which is the
// closest equivalent (foot → toe-base is enough for live retargeting).
inline constexpr HumanoidBoneMap kMixamoBoneMap = {
    "mixamorig:Hips",               // Hips
    "mixamorig:Spine",              // Spine
    "mixamorig:Spine1",             // Chest
    "mixamorig:Neck",               // Neck
    "mixamorig:Head",               // Head

    "mixamorig:LeftShoulder",       // LeftShoulder
    "mixamorig:LeftArm",            // LeftUpperArm
    "mixamorig:LeftForeArm",        // LeftLowerArm
    "mixamorig:LeftHand",           // LeftHand

    "mixamorig:RightShoulder",      // RightShoulder
    "mixamorig:RightArm",           // RightUpperArm
    "mixamorig:RightForeArm",       // RightLowerArm
    "mixamorig:RightHand",          // RightHand

    "mixamorig:LeftUpLeg",          // LeftUpperLeg
    "mixamorig:LeftLeg",            // LeftLowerLeg
    "mixamorig:LeftFoot",           // LeftFoot
    "mixamorig:LeftToeBase",        // LeftToes

    "mixamorig:RightUpLeg",         // RightUpperLeg
    "mixamorig:RightLeg",           // RightLowerLeg
    "mixamorig:RightFoot",          // RightFoot
    "mixamorig:RightToeBase",       // RightToes
};

// VRM — per the VRM humanoid spec bone names. Ready Player Me avatars
// also use this naming. Toes are not part of the mandatory VRM bone
// list but commonly present; we map to the optional "LeftToes"/"RightToes"
// which RPM and most VRM exports populate.
inline constexpr HumanoidBoneMap kVRMBoneMap = {
    "Hips",                  // Hips
    "Spine",                 // Spine
    "Chest",                 // Chest
    "Neck",                  // Neck
    "Head",                  // Head

    "LeftShoulder",          // LeftShoulder
    "LeftUpperArm",          // LeftUpperArm
    "LeftLowerArm",          // LeftLowerArm
    "LeftHand",              // LeftHand

    "RightShoulder",         // RightShoulder
    "RightUpperArm",         // RightUpperArm
    "RightLowerArm",         // RightLowerArm
    "RightHand",             // RightHand

    "LeftUpperLeg",          // LeftUpperLeg
    "LeftLowerLeg",          // LeftLowerLeg
    "LeftFoot",              // LeftFoot
    "LeftToes",              // LeftToes

    "RightUpperLeg",         // RightUpperLeg
    "RightLowerLeg",         // RightLowerLeg
    "RightFoot",             // RightFoot
    "RightToes",             // RightToes
};

// Unreal Mannequin — snake_case with "_l"/"_r" suffix. Spine is
// spine_01/02/03; we map Spine→spine_01, Chest→spine_02 (the visible
// chest bone). UE mannequin has no UpperChest; Spine→spine_03 would
// be closer if the rig has one authored. ball_l/r is the UE name for
// toes-equivalent.
inline constexpr HumanoidBoneMap kUnrealMannequinBoneMap = {
    "pelvis",                // Hips
    "spine_01",              // Spine
    "spine_02",              // Chest
    "neck_01",               // Neck
    "head",                  // Head

    "clavicle_l",            // LeftShoulder
    "upperarm_l",            // LeftUpperArm
    "lowerarm_l",            // LeftLowerArm
    "hand_l",                // LeftHand

    "clavicle_r",            // RightShoulder
    "upperarm_r",            // RightUpperArm
    "lowerarm_r",            // RightLowerArm
    "hand_r",                // RightHand

    "thigh_l",               // LeftUpperLeg
    "calf_l",                // LeftLowerLeg
    "foot_l",                // LeftFoot
    "ball_l",                // LeftToes

    "thigh_r",               // RightUpperLeg
    "calf_r",                // RightLowerLeg
    "foot_r",                // RightFoot
    "ball_r",                // RightToes
};

inline constexpr const HumanoidBoneMap&
humanoidBoneMap(HumanoidRigPreset preset) noexcept
{
  switch(preset)
  {
    case HumanoidRigPreset::Mixamo:
      return kMixamoBoneMap;
    case HumanoidRigPreset::VRM:
      return kVRMBoneMap;
    case HumanoidRigPreset::UnrealMannequin:
      return kUnrealMannequinBoneMap;
    case HumanoidRigPreset::Count:
      break;
  }
  return kMixamoBoneMap;
}

} // namespace Threedim
