// Threedim humanoid pose / retarget family (HumanoidPose, HumanoidPresets,
// HumanoidSourceMaps, HumanoidSourceAdapters, HumanoidRetarget).
//
// All five units are header-only and pure state: keypoint/tracker streams in,
// parent-local quaternion poses out, and a scene_state -> scene_state filter on
// top. No GPU / QRhi is ever touched (HumanoidRetarget has no render-thread
// path at all), so everything here runs headless with no QApplication —
// the same app-free driving pattern as tests/threedim/Transform3DCompose.cpp.
//
// What is pinned, derived on paper:
//   - the three rig preset tables are internally consistent (22 entries,
//     no two canonical bones drive the same target joint, dispatch returns
//     the matching table) and every entry resolves on a rig built from it;
//   - the source-side tables are structurally sound: the bone tree is
//     topological in enum order, rest axes are unit vectors, keypoint
//     edges index inside their workflow's landmark count;
//   - shortest_arc really is the shortest arc (identity / 90 degree /
//     antiparallel cases checked against hand-computed quaternions);
//   - a keypoint frame laid out exactly along the canonical T-pose axes
//     converts to the identity humanoid_pose, and bending one joint
//     produces the paper-derived local quaternion on that bone and
//     identity on its child (the parent-local invariant);
//   - HumanoidRetarget's offset math q_tgt = q_tgt_rest * inv(q_src_rest)
//     * q_src_cur: calibrating on a rest frame then replaying it leaves
//     every target joint at its rest rotation; a bent source elbow lands
//     on exactly the preset-mapped joint with the hand-computed product;
//   - unmapped / missing joints and empty inputs degrade gracefully
//     (passthrough or skip, never a crash, never a slam to origin);
//   - root motion applies scaled hip deltas on top of the captured
//     target rest translation.

#include <Threedim/HumanoidRetarget.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>
#include <set>
#include <string>
#include <vector>

using Catch::Approx;
using namespace Threedim;

namespace
{
constexpr std::size_t N = std::size_t(humanoid_bone_index::Count);
constexpr float kS45 = 0.70710678f; // sin(45 deg) == cos(45 deg)

std::size_t idx(humanoid_bone_index b) noexcept
{
  return std::size_t(b);
}

// Rotate v by unit quaternion q (x,y,z,w): v' = v + 2 q_v x (q_v x v + w v).
// Independent of the product code's Hamilton-product implementation, so it
// can act as a geometric cross-check on shortest_arc's output.
std::array<float, 3> rotate_vec(const float q[4], const float v[3])
{
  const float qx = q[0], qy = q[1], qz = q[2], qw = q[3];
  const float cx = qy * v[2] - qz * v[1] + qw * v[0];
  const float cy = qz * v[0] - qx * v[2] + qw * v[1];
  const float cz = qx * v[1] - qy * v[0] + qw * v[2];
  return {
      v[0] + 2.f * (qy * cz - qz * cy),
      v[1] + 2.f * (qz * cx - qx * cz),
      v[2] + 2.f * (qx * cy - qy * cx)};
}

void check_quat(
    const humanoid_bone& b, float x, float y, float z, float w,
    float margin = 1e-5f)
{
  CHECK(b.qx == Approx(x).margin(margin));
  CHECK(b.qy == Approx(y).margin(margin));
  CHECK(b.qz == Approx(z).margin(margin));
  CHECK(b.qw == Approx(w).margin(margin));
}

void check_joint_quat(
    const ossia::skeleton_joint& j, float x, float y, float z, float w,
    float margin = 1e-5f)
{
  CHECK(j.rotation[0] == Approx(x).margin(margin));
  CHECK(j.rotation[1] == Approx(y).margin(margin));
  CHECK(j.rotation[2] == Approx(z).margin(margin));
  CHECK(j.rotation[3] == Approx(w).margin(margin));
}

// ---------------------------------------------------------------------------
// A synthetic BlazePose frame whose every derivable bone edge points exactly
// along kHumanoidRestAxis, so keypoints_to_humanoid_pose must produce the
// identity pose. Landmark indices per the table in HumanoidSourceMaps.hpp.
//
//   Spine        23->11 +Y      arms   left -X chain, right +X chain
//   Chest        11->12 +Y      legs   -Y chains
//   Neck         11->0  +Y      feet   ankle->heel -Y
//   Head         0->2   +Y      toes   ankle->foot_index +Z
// ---------------------------------------------------------------------------
keypoint_stream makeBlazeRestFrame()
{
  keypoint_stream s;
  s.keypoints.resize(33);
  for(auto& k : s.keypoints)
    k.confidence = 1.f;
  s.mean_confidence = 1.f;

  auto at = [&](int i, float x, float y, float z) {
    s.keypoints[std::size_t(i)].x = x;
    s.keypoints[std::size_t(i)].y = y;
    s.keypoints[std::size_t(i)].z = z;
  };

  at(23, 0.f, 0.f, 0.f);   // left_hip (also the hip-translation proxy)
  at(11, 0.f, 1.f, 0.f);   // left_shoulder : Spine +Y
  at(12, 0.f, 2.f, 0.f);   // right_shoulder: Chest +Y
  at(0, 0.f, 2.f, 0.f);    // nose          : Neck  +Y
  at(2, 0.f, 3.f, 0.f);    // left_eye      : Head  +Y

  at(13, -1.f, 1.f, 0.f);  // left_elbow : LeftUpperArm -X
  at(15, -2.f, 1.f, 0.f);  // left_wrist : LeftLowerArm -X
  at(19, -3.f, 1.f, 0.f);  // left_index : LeftHand     -X

  at(14, 1.f, 2.f, 0.f);   // right_elbow: RightUpperArm +X
  at(16, 2.f, 2.f, 0.f);   // right_wrist: RightLowerArm +X
  at(20, 3.f, 2.f, 0.f);   // right_index: RightHand     +X

  at(25, 0.f, -1.f, 0.f);  // left_knee : LeftUpperLeg -Y
  at(27, 0.f, -2.f, 0.f);  // left_ankle: LeftLowerLeg -Y
  at(29, 0.f, -3.f, 0.f);  // left_heel : LeftFoot     -Y
  at(31, 0.f, -2.f, 1.f);  // left_foot_index: LeftToes +Z

  at(24, 1.f, 0.f, 0.f);   // right_hip
  at(26, 1.f, -1.f, 0.f);  // right_knee
  at(28, 1.f, -2.f, 0.f);  // right_ankle
  at(30, 1.f, -3.f, 0.f);  // right_heel
  at(32, 1.f, -2.f, 1.f);  // right_foot_index
  return s;
}

// Same frame, left elbow bent 90 degrees: forearm and hand hang straight
// down (-Y) instead of pointing -X.
keypoint_stream makeBlazeBentElbowFrame()
{
  keypoint_stream s = makeBlazeRestFrame();
  s.keypoints[15] = {-1.f, 0.f, 0.f, 1.f};  // left_wrist below the elbow
  s.keypoints[19] = {-1.f, -1.f, 0.f, 1.f}; // left_index below the wrist
  return s;
}

void translate(keypoint_stream& s, float dx, float dy, float dz)
{
  for(auto& k : s.keypoints)
  {
    k.x += dx;
    k.y += dy;
    k.z += dz;
  }
}

// The set of bones that CAN be derived from a BlazePose frame: everything
// except the root (no edge) and the two degenerate collar bones.
bool blaze_derivable(std::size_t b)
{
  return b != idx(humanoid_bone_index::Hips)
         && b != idx(humanoid_bone_index::LeftShoulder)
         && b != idx(humanoid_bone_index::RightShoulder);
}

// ---------------------------------------------------------------------------
// Scene builders (same shape as Transform3DCompose.cpp's helpers).
// ---------------------------------------------------------------------------
std::shared_ptr<ossia::skeleton_component>
makeSkeleton(const std::vector<std::string>& names)
{
  auto skel = std::make_shared<ossia::skeleton_component>();
  for(const auto& n : names)
  {
    ossia::skeleton_joint j; // rotation {0,0,0,1}, scale {1,1,1}
    j.name = n;
    skel->joints.push_back(j);
  }
  return skel;
}

std::shared_ptr<ossia::skeleton_component>
makeSkeletonFromMap(const HumanoidBoneMap& map)
{
  std::vector<std::string> names;
  for(const auto& n : map)
    if(!n.empty())
      names.emplace_back(n);
  return makeSkeleton(names);
}

std::shared_ptr<ossia::scene_state>
makeScene(std::shared_ptr<ossia::skeleton_component> skel)
{
  auto root = std::make_shared<ossia::scene_node>();
  root->id.value = 1;
  root->name = "rig";

  auto st = std::make_shared<ossia::scene_state>();
  st->roots = std::make_shared<std::vector<ossia::scene_node_ptr>>(
      std::vector<ossia::scene_node_ptr>{root});
  st->skeletons
      = std::make_shared<std::vector<ossia::skeleton_component_ptr>>(
          std::vector<ossia::skeleton_component_ptr>{skel});
  st->version = 1;
  return st;
}

const ossia::skeleton_component* outputSkeleton(const HumanoidRetarget& n)
{
  const auto& st = n.outputs.scene_out.scene.state;
  if(!st || !st->skeletons || st->skeletons->empty())
    return nullptr;
  return (*st->skeletons)[0].get();
}
} // namespace

// ============================================================ preset tables

TEST_CASE(
    "rig presets are internally consistent bone-name tables",
    "[threedim][humanoid][presets]")
{
  struct
  {
    const char* label;
    const HumanoidBoneMap* map;
  } presets[]
      = {{"Mixamo", &kMixamoBoneMap},
         {"VRM", &kVRMBoneMap},
         {"UnrealMannequin", &kUnrealMannequinBoneMap}};

  for(const auto& p : presets)
  {
    INFO("preset " << p.label);
    const auto& m = *p.map;

    // The retargeter needs a Hips entry for root motion in every preset.
    CHECK_FALSE(m[idx(humanoid_bone_index::Hips)].empty());

    // No two canonical bones may drive the same target joint: a duplicate
    // makes the later bone silently overwrite the earlier one's rotation.
    std::set<std::string_view> seen;
    for(std::size_t b = 0; b < N; ++b)
    {
      if(m[b].empty())
        continue; // an empty entry is the documented "rig lacks this bone"
      INFO("bone index " << b << " -> \"" << m[b] << "\"");
      CHECK(seen.insert(m[b]).second);
    }
  }

  // Dispatch returns the matching table, and the out-of-range sentinel
  // falls back to Mixamo rather than reading past the enum.
  CHECK(&humanoidBoneMap(HumanoidRigPreset::Mixamo) == &kMixamoBoneMap);
  CHECK(&humanoidBoneMap(HumanoidRigPreset::VRM) == &kVRMBoneMap);
  CHECK(
      &humanoidBoneMap(HumanoidRigPreset::UnrealMannequin)
      == &kUnrealMannequinBoneMap);
  CHECK(&humanoidBoneMap(HumanoidRigPreset::Count) == &kMixamoBoneMap);
}

TEST_CASE(
    "every preset entry resolves on a rig named from that preset",
    "[threedim][humanoid][presets]")
{
  // End-to-end through calibrate(): build a skeleton whose joints carry the
  // preset's names, run one frame, then every canonical bone must have
  // found a joint — and found the RIGHT one (name match).
  const HumanoidRigPreset all[]
      = {HumanoidRigPreset::Mixamo, HumanoidRigPreset::VRM,
         HumanoidRigPreset::UnrealMannequin};
  for(auto p : all)
  {
    INFO("preset " << int(p));
    const auto& map = humanoidBoneMap(p);

    auto skel = makeSkeletonFromMap(map);
    HumanoidRetarget n;
    n.inputs.scene_in.scene.state = makeScene(skel);
    n.inputs.source.value = HumanoidSourceType::BlazePose;
    n.inputs.preset.value = p;
    n.inputs.keypoints_in.value = makeBlazeRestFrame();

    n();

    REQUIRE(n.m_calibrated);
    for(std::size_t b = 0; b < N; ++b)
    {
      INFO("bone index " << b << " -> \"" << map[b] << "\"");
      const int32_t got = n.m_target_joint_indices[b];
      if(map[b].empty())
      {
        CHECK(got == -1);
      }
      else
      {
        REQUIRE(got >= 0);
        REQUIRE(got < int32_t(skel->joints.size()));
        CHECK(skel->joints[got].name == map[b]);
      }
    }
  }
}

// ======================================================== source-side tables

TEST_CASE(
    "bone tree and rest axes are structurally sound",
    "[threedim][humanoid][sourcemaps]")
{
  // Hips is the sole root; every other bone's parent strictly precedes it,
  // which is exactly the property the adapters' single forward pass
  // (world -> parent-local) depends on.
  CHECK(kHumanoidParent[idx(humanoid_bone_index::Hips)]
        == humanoid_bone_index::Count);
  for(std::size_t b = 1; b < N; ++b)
  {
    INFO("bone index " << b);
    CHECK(kHumanoidParent[b] != humanoid_bone_index::Count);
    CHECK(idx(kHumanoidParent[b]) < b);
  }

  // Rest axes: zero for the root, unit for everything else (shortest_arc
  // assumes unit "from" vectors).
  for(std::size_t b = 0; b < N; ++b)
  {
    const auto& a = kHumanoidRestAxis[b];
    const float len2 = a[0] * a[0] + a[1] * a[1] + a[2] * a[2];
    INFO("bone index " << b);
    if(b == idx(humanoid_bone_index::Hips))
      CHECK(len2 == 0.f);
    else
      CHECK(len2 == Approx(1.f).margin(1e-6));
  }
}

TEST_CASE(
    "keypoint maps index inside their workflow's landmark count",
    "[threedim][humanoid][sourcemaps]")
{
  for(std::size_t b = 0; b < N; ++b)
  {
    INFO("bone index " << b);
    const auto& blaze = kBlazePoseMap[b];
    if(blaze.valid())
    {
      CHECK(blaze.parent_idx < 33);
      CHECK(blaze.child_idx < 33);
    }
    const auto& coco = kCoco17Map[b];
    if(coco.valid())
    {
      CHECK(coco.parent_idx < 17);
      CHECK(coco.child_idx < 17);
    }
    // RTMPose Whole reuses the COCO body subset verbatim.
    CHECK(kRTMPoseWholeMap[b].parent_idx == coco.parent_idx);
    CHECK(kRTMPoseWholeMap[b].child_idx == coco.child_idx);
  }

  // The root has no edge in any workflow (its rotation is not derivable
  // from two landmarks), and BlazePose's collar entries are the documented
  // degenerate parent==child markers that the adapter must skip.
  CHECK_FALSE(kBlazePoseMap[idx(humanoid_bone_index::Hips)].valid());
  CHECK_FALSE(kCoco17Map[idx(humanoid_bone_index::Hips)].valid());
  const auto& lsh = kBlazePoseMap[idx(humanoid_bone_index::LeftShoulder)];
  const auto& rsh = kBlazePoseMap[idx(humanoid_bone_index::RightShoulder)];
  CHECK(lsh.parent_idx == lsh.child_idx);
  CHECK(rsh.parent_idx == rsh.child_idx);
}

// =============================================================== shortest_arc

TEST_CASE(
    "shortest_arc matches hand-computed quaternions",
    "[threedim][humanoid][adapters]")
{
  float q[4];

  SECTION("aligned vectors give the identity")
  {
    const float v[3] = {0.f, 1.f, 0.f};
    shortest_arc(v, v, q);
    CHECK(q[0] == 0.f);
    CHECK(q[1] == 0.f);
    CHECK(q[2] == 0.f);
    CHECK(q[3] == 1.f);
  }

  SECTION("-X to -Y is 90 degrees about +Z")
  {
    // cross(-X, -Y) = +Z, dot = 0 -> q = (0, 0, sin45, cos45).
    const float from[3] = {-1.f, 0.f, 0.f};
    const float to[3] = {0.f, -1.f, 0.f};
    shortest_arc(from, to, q);
    CHECK(q[0] == Approx(0.f).margin(1e-6));
    CHECK(q[1] == Approx(0.f).margin(1e-6));
    CHECK(q[2] == Approx(kS45).margin(1e-6));
    CHECK(q[3] == Approx(kS45).margin(1e-6));
  }

  SECTION("antiparallel input yields a unit 180-degree turn that works")
  {
    const float from[3] = {0.f, 1.f, 0.f};
    const float to[3] = {0.f, -1.f, 0.f};
    shortest_arc(from, to, q);
    CHECK(q[3] == 0.f); // pure 180 degrees
    CHECK(
        q[0] * q[0] + q[1] * q[1] + q[2] * q[2] == Approx(1.f).margin(1e-6));
    const auto r = rotate_vec(q, from);
    CHECK(r[0] == Approx(to[0]).margin(1e-5));
    CHECK(r[1] == Approx(to[1]).margin(1e-5));
    CHECK(r[2] == Approx(to[2]).margin(1e-5));
  }

  SECTION("general case: q really maps from onto to")
  {
    const float from[3] = {1.f, 0.f, 0.f};
    const float inv3 = 1.f / std::sqrt(3.f);
    const float to[3] = {inv3, inv3, inv3};
    shortest_arc(from, to, q);
    const auto r = rotate_vec(q, from);
    CHECK(r[0] == Approx(to[0]).margin(1e-5));
    CHECK(r[1] == Approx(to[1]).margin(1e-5));
    CHECK(r[2] == Approx(to[2]).margin(1e-5));
  }
}

// ================================================ keypoints -> humanoid_pose

TEST_CASE(
    "a canonical-rest keypoint frame converts to the identity pose",
    "[threedim][humanoid][adapters]")
{
  const auto pose
      = keypoints_to_humanoid_pose(makeBlazeRestFrame(), kBlazePoseMap, 0.5f);

  for(std::size_t b = 0; b < N; ++b)
  {
    INFO("bone index " << b);
    const auto& bone = pose.bones[b];
    if(blaze_derivable(b))
    {
      CHECK(bone.validity == 1.f);
      check_quat(bone, 0.f, 0.f, 0.f, 1.f);
    }
    else
    {
      // Root + degenerate collars: not derivable, flagged invalid so the
      // retargeter keeps the target's rest rotation.
      CHECK(bone.validity == 0.f);
      check_quat(bone, 0.f, 0.f, 0.f, 1.f);
    }
  }

  // Hip proxy = the Spine edge's parent landmark (left_hip, index 23).
  CHECK(pose.hip_x == 0.f);
  CHECK(pose.hip_y == 0.f);
  CHECK(pose.hip_z == 0.f);
}

TEST_CASE(
    "a bent elbow becomes a parent-local 90-degree Z on exactly that bone",
    "[threedim][humanoid][adapters]")
{
  const auto pose = keypoints_to_humanoid_pose(
      makeBlazeBentElbowFrame(), kBlazePoseMap, 0.5f);

  // World: forearm turned from -X to -Y = 90 degrees about +Z; its parent
  // (upper arm) is unrotated, so the local rotation equals the world one.
  check_quat(
      pose[humanoid_bone_index::LeftLowerArm], 0.f, 0.f, kS45, kS45, 1e-5f);

  // The hand follows its parent rigidly (wrist->index also points -Y), so
  // its PARENT-LOCAL rotation is the identity. This is the invariant the
  // whole retarget contract rests on: rotations compose down the chain.
  check_quat(pose[humanoid_bone_index::LeftHand], 0.f, 0.f, 0.f, 1.f, 1e-5f);

  // Nothing else moved.
  check_quat(pose[humanoid_bone_index::LeftUpperArm], 0.f, 0.f, 0.f, 1.f);
  check_quat(pose[humanoid_bone_index::RightLowerArm], 0.f, 0.f, 0.f, 1.f);
  check_quat(pose[humanoid_bone_index::Spine], 0.f, 0.f, 0.f, 1.f);
}

TEST_CASE(
    "low-confidence and short keypoint streams degrade to invalid bones",
    "[threedim][humanoid][adapters]")
{
  SECTION("a keypoint below the threshold invalidates only its bones")
  {
    auto frame = makeBlazeRestFrame();
    frame.keypoints[13].confidence = 0.1f; // left_elbow
    const auto pose
        = keypoints_to_humanoid_pose(frame, kBlazePoseMap, 0.5f);
    // Both edges touching landmark 13 drop out...
    CHECK(pose[humanoid_bone_index::LeftUpperArm].validity == 0.f);
    CHECK(pose[humanoid_bone_index::LeftLowerArm].validity == 0.f);
    // ...their invalid quats stay a safe identity...
    check_quat(pose[humanoid_bone_index::LeftUpperArm], 0.f, 0.f, 0.f, 1.f);
    // ...and unrelated chains keep tracking.
    CHECK(pose[humanoid_bone_index::RightUpperArm].validity == 1.f);
    CHECK(pose[humanoid_bone_index::LeftHand].validity == 1.f);
  }

  SECTION("a truncated stream never reads out of range")
  {
    auto frame = makeBlazeRestFrame();
    frame.keypoints.resize(17); // COCO-sized buffer fed to the Blaze map
    const auto pose
        = keypoints_to_humanoid_pose(frame, kBlazePoseMap, 0.5f);
    // Edges whose landmarks are >= 17 must come back invalid, not crash.
    CHECK(pose[humanoid_bone_index::LeftUpperLeg].validity == 0.f);
    CHECK(pose[humanoid_bone_index::LeftHand].validity == 0.f);
    // Spine (23 -> 11) is also out of range now.
    CHECK(pose[humanoid_bone_index::Spine].validity == 0.f);
  }

  SECTION("an empty stream is all-invalid")
  {
    const auto pose
        = keypoints_to_humanoid_pose(keypoint_stream{}, kBlazePoseMap, 0.5f);
    for(std::size_t b = 0; b < N; ++b)
      CHECK(pose.bones[b].validity == 0.f);
    CHECK(pose.hip_x == 0.f);
  }
}

// ================================================= trackers -> humanoid_pose

TEST_CASE(
    "six-tracker bundles drive exactly their six bones",
    "[threedim][humanoid][adapters]")
{
  tracker_bundle_6 t{};
  // Hips: 90 degrees about +Z, position (1,2,3), tracked.
  t.hips = {1.f, 2.f, 3.f, 0.f, 0.f, kS45, kS45, 1.f};
  // Head: identity rotation, tracked.
  t.head = {0.f, 1.7f, 0.f, 0.f, 0.f, 0.f, 1.f, 1.f};
  // Left hand: valid rotation but tracker confidence below 0.5 -> ignored.
  t.left_hand = {0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.2f};
  // right_hand / feet stay validity 0 (offline).

  const auto pose = trackers_to_humanoid_pose(t);

  // Hips is the root: its world rotation IS its local rotation.
  CHECK(pose[humanoid_bone_index::Hips].validity == 1.f);
  check_quat(pose[humanoid_bone_index::Hips], 0.f, 0.f, kS45, kS45);

  // Head's parent chain (Neck) has no tracker, so the world rotation is
  // emitted as local — the documented degradation that keeps the head
  // visible instead of composing against a missing parent.
  CHECK(pose[humanoid_bone_index::Head].validity == 1.f);
  check_quat(pose[humanoid_bone_index::Head], 0.f, 0.f, 0.f, 1.f);

  // The occluded hand and every undriven bone stay invalid, at identity —
  // lost tracking must freeze the bone, not slam it anywhere.
  CHECK(pose[humanoid_bone_index::LeftHand].validity == 0.f);
  check_quat(pose[humanoid_bone_index::LeftHand], 0.f, 0.f, 0.f, 1.f);
  CHECK(pose[humanoid_bone_index::Spine].validity == 0.f);
  CHECK(pose[humanoid_bone_index::LeftUpperArm].validity == 0.f);

  // Hip world position rides along for root motion.
  CHECK(pose.hip_x == 1.f);
  CHECK(pose.hip_y == 2.f);
  CHECK(pose.hip_z == 3.f);

  // An offline hips tracker leaves the hip position at the origin.
  t.hips.validity = 0.f;
  const auto pose2 = trackers_to_humanoid_pose(t);
  CHECK(pose2[humanoid_bone_index::Hips].validity == 0.f);
  CHECK(pose2.hip_x == 0.f);
}

// ========================================================== HumanoidRetarget

TEST_CASE(
    "replaying the calibration frame leaves the target rig at rest",
    "[threedim][humanoid][retarget]")
{
  auto skel = makeSkeletonFromMap(kVRMBoneMap);
  // A distinct rest rotation on one joint proves "unchanged" is not just
  // "identity in, identity out".
  const int32_t lowerArm = skel->find_joint("LeftLowerArm");
  REQUIRE(lowerArm >= 0);
  skel->joints[lowerArm].rotation[0] = kS45; // 90 degrees about +X rest
  skel->joints[lowerArm].rotation[3] = kS45;

  auto in = makeScene(skel);
  HumanoidRetarget n;
  n.inputs.scene_in.scene.state = in;
  n.inputs.source.value = HumanoidSourceType::BlazePose;
  n.inputs.preset.value = HumanoidRigPreset::VRM;
  n.inputs.keypoints_in.value = makeBlazeRestFrame();

  n(); // calibrates on this frame, then retargets the very same frame

  const auto out = n.outputs.scene_out.scene.state;
  REQUIRE(out);
  CHECK(n.outputs.scene_out.dirty == ossia::scene_port::dirty_transform);

  // The skeleton is CLONED, never mutated in place — other consumers of the
  // input scene must not see our joints (the InverseKinematics pattern).
  const auto* outSkel = outputSkeleton(n);
  REQUIRE(outSkel);
  CHECK(outSkel != skel.get());
  CHECK(skel->joints[lowerArm].rotation[2] == 0.f); // input untouched

  // delta = inv(rest) * rest = identity, so q_tgt = q_tgt_rest everywhere.
  REQUIRE(outSkel->joints.size() == skel->joints.size());
  for(std::size_t j = 0; j < skel->joints.size(); ++j)
  {
    INFO("joint " << j << " (" << skel->joints[j].name << ")");
    for(int c = 0; c < 4; ++c)
      CHECK(
          outSkel->joints[j].rotation[c]
          == Approx(skel->joints[j].rotation[c]).margin(1e-5));
  }

  // Non-skeleton state is shared, not copied; bookkeeping advances.
  CHECK(out->roots.get() == in->roots.get());
  CHECK(out->version == 1);
  CHECK(out->dirty_index == in->dirty_index + 1);
  CHECK(outSkel->dirty_index == skel->dirty_index + 1);
}

TEST_CASE(
    "a bent source elbow lands on the mapped joint as rest * delta",
    "[threedim][humanoid][retarget]")
{
  auto skel = makeSkeletonFromMap(kVRMBoneMap);
  const int32_t lowerArm = skel->find_joint("LeftLowerArm");
  const int32_t upperArm = skel->find_joint("LeftUpperArm");
  const int32_t hand = skel->find_joint("LeftHand");
  REQUIRE(lowerArm >= 0);
  REQUIRE(upperArm >= 0);
  REQUIRE(hand >= 0);
  // Target rest for the forearm: 90 degrees about +X.
  skel->joints[lowerArm].rotation[0] = kS45;
  skel->joints[lowerArm].rotation[3] = kS45;

  HumanoidRetarget n;
  n.inputs.scene_in.scene.state = makeScene(skel);
  n.inputs.source.value = HumanoidSourceType::BlazePose;
  n.inputs.preset.value = HumanoidRigPreset::VRM;

  // Frame 1: T-pose — capture rests.
  n.inputs.keypoints_in.value = makeBlazeRestFrame();
  n();
  REQUIRE(n.m_calibrated);

  // Frame 2: elbow bent 90 degrees.
  n.inputs.keypoints_in.value = makeBlazeBentElbowFrame();
  n();

  const auto* outSkel = outputSkeleton(n);
  REQUIRE(outSkel);

  // Source delta on LeftLowerArm = (0,0,s,c), 90 degrees about +Z.
  // q_tgt = q_rest * delta = (s,0,0,c)*(0,0,s,c), Hamilton, s=c=sqrt(2)/2:
  //   x = c*0 + s*c + 0*s - 0*0 = 1/2      y = c*0 - s*s + 0*c + 0*0 = -1/2
  //   z = c*s + s*0 - 0*0 + 0*c = 1/2      w = c*c - s*0 - 0*0 - 0*s = 1/2
  check_joint_quat(outSkel->joints[lowerArm], 0.5f, -0.5f, 0.5f, 0.5f, 1e-5f);

  // The hand's parent-local delta is identity, so it holds its rest; the
  // upper arm did not move either.
  check_joint_quat(outSkel->joints[hand], 0.f, 0.f, 0.f, 1.f, 1e-5f);
  check_joint_quat(outSkel->joints[upperArm], 0.f, 0.f, 0.f, 1.f, 1e-5f);

  // Second retarget frame: version keeps counting.
  CHECK(n.outputs.scene_out.scene.state->version == 2);
}

TEST_CASE(
    "invalid source bones and unresolved joints are skipped, never slammed",
    "[threedim][humanoid][retarget]")
{
  SECTION("bones with validity 0 keep the target's rest rotation")
  {
    auto skel = makeSkeletonFromMap(kVRMBoneMap);
    // Hips has no keypoint edge -> validity 0 -> its distinctive rest
    // rotation must survive retargeting untouched.
    const int32_t hips = skel->find_joint("Hips");
    REQUIRE(hips >= 0);
    skel->joints[hips].rotation[1] = kS45; // 90 degrees about +Y
    skel->joints[hips].rotation[3] = kS45;

    HumanoidRetarget n;
    n.inputs.scene_in.scene.state = makeScene(skel);
    n.inputs.source.value = HumanoidSourceType::BlazePose;
    n.inputs.preset.value = HumanoidRigPreset::VRM;
    n.inputs.keypoints_in.value = makeBlazeBentElbowFrame();
    n();

    const auto* outSkel = outputSkeleton(n);
    REQUIRE(outSkel);
    check_joint_quat(outSkel->joints[hips], 0.f, kS45, 0.f, kS45, 1e-5f);
  }

  SECTION("a rig with almost no matching joints still retargets the rest")
  {
    // Only the forearm exists; 21 canonical bones fail find_joint. This
    // must not crash and must still drive the one resolvable joint.
    auto skel = makeSkeleton({"LeftLowerArm"});
    HumanoidRetarget n;
    n.inputs.scene_in.scene.state = makeScene(skel);
    n.inputs.source.value = HumanoidSourceType::BlazePose;
    n.inputs.preset.value = HumanoidRigPreset::VRM;
    n.inputs.keypoints_in.value = makeBlazeRestFrame();
    n();
    n.inputs.keypoints_in.value = makeBlazeBentElbowFrame();
    n();

    const auto* outSkel = outputSkeleton(n);
    REQUIRE(outSkel);
    REQUIRE(outSkel->joints.size() == 1u);
    check_joint_quat(outSkel->joints[0], 0.f, 0.f, kS45, kS45, 1e-5f);
  }
}

TEST_CASE(
    "Off, missing data and skeleton-less scenes pass through untouched",
    "[threedim][humanoid][retarget]")
{
  SECTION("null input scene clears the output")
  {
    HumanoidRetarget n;
    n();
    CHECK(n.outputs.scene_out.scene.state == nullptr);
    CHECK(n.outputs.scene_out.dirty == 0);
  }

  SECTION("source Off is an identity passthrough")
  {
    HumanoidRetarget n;
    auto in = makeScene(makeSkeletonFromMap(kVRMBoneMap));
    n.inputs.scene_in.scene.state = in;
    n.inputs.source.value = HumanoidSourceType::Off;
    n.inputs.keypoints_in.value = makeBlazeRestFrame();
    n();
    CHECK(n.outputs.scene_out.scene.state.get() == in.get());
    CHECK(n.outputs.scene_out.dirty == 0);
  }

  SECTION("a source with no fresh data passes through")
  {
    HumanoidRetarget n;
    auto in = makeScene(makeSkeletonFromMap(kVRMBoneMap));
    n.inputs.scene_in.scene.state = in;
    n.inputs.source.value = HumanoidSourceType::BlazePose;
    n.inputs.keypoints_in.value = std::nullopt;
    n();
    CHECK(n.outputs.scene_out.scene.state.get() == in.get());
    CHECK(n.outputs.scene_out.dirty == 0);
  }

  SECTION("a scene without skeletons passes through")
  {
    HumanoidRetarget n;
    auto in = makeScene(makeSkeletonFromMap(kVRMBoneMap));
    in->skeletons.reset();
    n.inputs.scene_in.scene.state = in;
    n.inputs.source.value = HumanoidSourceType::BlazePose;
    n.inputs.keypoints_in.value = makeBlazeRestFrame();
    n();
    CHECK(n.outputs.scene_out.scene.state.get() == in.get());
    CHECK(n.outputs.scene_out.dirty == 0);
  }
}

TEST_CASE(
    "root motion applies the scaled hip delta on top of the target rest",
    "[threedim][humanoid][retarget]")
{
  auto skel = makeSkeletonFromMap(kVRMBoneMap);
  const int32_t hips = skel->find_joint("Hips");
  REQUIRE(hips >= 0);
  skel->joints[hips].translation[1] = 5.f; // target rest hip height

  HumanoidRetarget n;
  n.inputs.scene_in.scene.state = makeScene(skel);
  n.inputs.source.value = HumanoidSourceType::BlazePose;
  n.inputs.preset.value = HumanoidRigPreset::VRM;
  n.inputs.root_motion.value = true;
  n.inputs.root_scale.value = 2.f;

  // Frame 1: calibrate; source rest hip = left_hip landmark = (0,0,0).
  n.inputs.keypoints_in.value = makeBlazeRestFrame();
  n();

  // Frame 2: the whole performer walks by (1,2,3) — a rigid translation
  // changes no bone direction, only the hip proxy landmark.
  auto walked = makeBlazeRestFrame();
  translate(walked, 1.f, 2.f, 3.f);
  n.inputs.keypoints_in.value = walked;
  n();

  const auto* outSkel = outputSkeleton(n);
  REQUIRE(outSkel);
  // rest_tr + (hip_cur - hip_rest) * scale = (0,5,0) + (1,2,3)*2.
  CHECK(outSkel->joints[hips].translation[0] == Approx(2.f).margin(1e-5));
  CHECK(outSkel->joints[hips].translation[1] == Approx(9.f).margin(1e-5));
  CHECK(outSkel->joints[hips].translation[2] == Approx(6.f).margin(1e-5));

  // With the toggle off, the same walk leaves the hip at its rest place.
  HumanoidRetarget still;
  still.inputs.scene_in.scene.state = makeScene(skel);
  still.inputs.source.value = HumanoidSourceType::BlazePose;
  still.inputs.preset.value = HumanoidRigPreset::VRM;
  still.inputs.keypoints_in.value = makeBlazeRestFrame();
  still();
  still.inputs.keypoints_in.value = walked;
  still();
  const auto* stillSkel = outputSkeleton(still);
  REQUIRE(stillSkel);
  CHECK(stillSkel->joints[hips].translation[0] == 0.f);
  CHECK(stillSkel->joints[hips].translation[1] == 5.f);
  CHECK(stillSkel->joints[hips].translation[2] == 0.f);
}
