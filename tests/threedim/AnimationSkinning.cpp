// L3 regression guard — split/threedim finding #9 (AnimationPlayer never
// deforms skinned meshes: it wrote an ignored buffer and left joint TRS at
// bind pose).
//
// The skinning block recomputed joint_matrix = world x inverse_bind into
// skeleton_component::joint_matrices_buffer, but the sole skinning consumer
// (SceneGPUState) ALWAYS derives its joint matrices from joints[].{translation,
// rotation,scale} via its own forward kinematics and never reads
// joint_matrices_buffer. AnimationPlayer meanwhile animated only the rigid
// scene_transform payloads and never touched joints[] — so the animated pose
// never reached the skinning path and skinned meshes stayed frozen in bind
// pose.
//
// The fix writes each sampled TRS override (keyed by scene_node id) into the
// cloned skeleton's joints[] local TRS, mapped joint -> node via
// joint_node_ids, and bumps the skeleton dirty_index.
//
// This is a pure state test (no GPU): we build a scene with one animated
// rotation channel targeting the node that backs joint 0, tick once, and read
// the OUTPUT scene's skeleton. Post-fix, joints[0].rotation has moved off the
// bind-pose identity quaternion to the sampled value; pre-fix it stays at the
// bind pose (0,0,0,1) (RED). We observe the published skeleton_component state,
// not a rendered image.

#include <Threedim/AnimationPlayer.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>
#include <vector>

namespace
{
// scene_node id that backs the animated joint.
constexpr uint64_t kJointNodeId = 77;

// A 90 degrees rotation about +Z as a glTF (x,y,z,w) quaternion.
constexpr float kQz = 0.70710678f; // sin(45deg) == cos(45deg)

std::shared_ptr<ossia::scene_state> makeSkinnedScene()
{
  // Minimal non-empty root (scene_state::empty() gates AnimationPlayer). The
  // root does not need to be the joint node — the skinning path resolves joints
  // via joint_node_ids against the sampled overrides, independent of the tree.
  auto root = std::make_shared<ossia::scene_node>();
  root->id.value = 1;
  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  roots->push_back(root);

  // One rotation channel on node 77, constant 90deg-about-Z at both keyframes,
  // so any sampled t yields the same non-identity quaternion.
  auto anim = std::make_shared<ossia::animation_component>();
  anim->duration = 1.f;
  ossia::animation_channel ch;
  ch.target_node_id = kJointNodeId;
  ch.target_path = ossia::animation_target::rotation;
  ch.interpolation = ossia::animation_interpolation::linear;
  ch.times = std::make_shared<std::vector<float>>(std::vector<float>{0.f, 1.f});
  ch.values = std::make_shared<std::vector<float>>(
      std::vector<float>{0.f, 0.f, kQz, kQz, 0.f, 0.f, kQz, kQz});
  anim->channels.push_back(ch);
  auto anims
      = std::make_shared<std::vector<ossia::animation_component_ptr>>();
  anims->push_back(anim);

  // One skeleton, one joint at bind pose (identity rotation), mapped to node 77.
  auto skel = std::make_shared<ossia::skeleton_component>();
  ossia::skeleton_joint joint; // defaults: rotation {0,0,0,1}, scale {1,1,1}
  joint.name = "j0";
  skel->joints.push_back(joint);
  skel->joint_node_ids.push_back(ossia::scene_node_id{kJointNodeId});
  skel->dirty_index = 0;
  auto skels
      = std::make_shared<std::vector<ossia::skeleton_component_ptr>>();
  skels->push_back(skel);

  auto st = std::make_shared<ossia::scene_state>();
  st->roots = roots;
  st->animations = anims;
  st->skeletons = skels;
  return st;
}

const ossia::skeleton_component*
outputSkeleton(Threedim::AnimationPlayer& node)
{
  const auto& out = node.outputs.scene_out.scene.state;
  if(!out || !out->skeletons || out->skeletons->empty())
    return nullptr;
  return (*out->skeletons)[0].get();
}
} // namespace

TEST_CASE(
    "AnimationPlayer writes sampled TRS into skeleton joints",
    "[threedim][animation][f9]")
{
  Threedim::AnimationPlayer node;
  node.inputs.scene_in.scene.state = makeSkinnedScene();
  node.inputs.time.value = 0.f;
  node.inputs.speed.value = 1.f;
  node.inputs.loop.value = false;
  node.inputs.clip_index.value = -1;

  node();

  const ossia::skeleton_component* skel = outputSkeleton(node);
  REQUIRE(skel != nullptr);
  REQUIRE(skel->joints.size() == 1u);

  // The fix: the joint's LOCAL rotation TRS (the data SceneGPUState's forward
  // kinematics actually consumes) is now the sampled 90deg-about-Z quaternion,
  // no longer the bind-pose identity. Pre-fix the joint stays at bind pose and
  // both component checks fail (RED).
  const auto& r = skel->joints[0].rotation; // (x,y,z,w)
  CHECK(std::abs(r[2] - kQz) < 1e-4f);
  CHECK(std::abs(r[3] - kQz) < 1e-4f);

  // It has genuinely moved off the bind-pose identity (0,0,0,1).
  const bool movedFromBind
      = std::abs(r[0]) > 1e-4f || std::abs(r[1]) > 1e-4f
        || std::abs(r[2]) > 1e-4f || std::abs(r[3] - 1.f) > 1e-4f;
  CHECK(movedFromBind);

  // And the skeleton is flagged dirty so the renderer re-runs FK.
  CHECK(skel->dirty_index > 0);
}
