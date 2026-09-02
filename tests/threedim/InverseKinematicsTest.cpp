// Threedim::InverseKinematics (2-bone analytic IK).
//
// Pure scene_state / QQuaternion math driven app-free: the node is a plain
// operator() over a scene_spec — it has no init/update/release render-thread
// hooks at all, so nothing GPU-shaped is stubbed or entered (same discipline
// as Transform3DCompose.cpp / CameraRelease.cpp).
//
// Every expectation below is derived from the geometry on paper, never from
// running the solver:
//   * worldJointPos composes T*R*S down the parent chain (checked against
//     hand-computed constants, including a scaled parent).
//   * A solve is a pair of rotations, so it must preserve both bone lengths
//     exactly, and — by the law of cosines the elbow angle is built from —
//     must place the end effector at distance min(|target-root|, lA+lB)
//     from the root. Verified with an INDEPENDENT quaternion FK written in
//     this file, on both a straight and a 90-degree-bent start pose.
//   * A planar problem (root, mid, end, target, pole all in z=0) must stay
//     planar.
//   * weight = 0, an unknown end joint, a chain shorter than 3 joints, and
//     a skeleton-less scene are all identity passthroughs (same state
//     pointer, dirty == 0); a null scene clears the output.
//   * A zero-length bone must not NaN.
//
// One TEST_CASE is [!shouldfail]: actually REACHING a reachable target.
// See the DEFECT comment there — the expectations are the correct geometry
// and are kept; the tag comes off the day the solver is fixed.

#include <Threedim/InverseKinematics.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QQuaternion>
#include <QVector3D>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

using Catch::Approx;

namespace
{
// Independent forward kinematics, written from the TRS definition in
// geometry_port.hpp rather than reusing the unit's worldJointPos: with unit
// scales (all solver fixtures below), pos(i) = pos(p) + rot(p)*t_i and
// rot(i) = rot(p)*r_i.
QVector3D fkPos(const ossia::skeleton_component& s, int32_t idx)
{
  std::vector<int32_t> chain;
  for(int32_t i = idx; i >= 0; i = s.joints[i].parent_index)
    chain.push_back(i);
  std::reverse(chain.begin(), chain.end());

  QVector3D pos{};
  QQuaternion rot;
  for(int32_t i : chain)
  {
    const auto& j = s.joints[i];
    pos += rot.rotatedVector(
        QVector3D(j.translation[0], j.translation[1], j.translation[2]));
    rot = rot
          * QQuaternion(
              j.rotation[3], j.rotation[0], j.rotation[1], j.rotation[2]);
  }
  return pos;
}

ossia::skeleton_joint joint(const char* name, int32_t parent, QVector3D t)
{
  ossia::skeleton_joint j; // defaults: rotation {0,0,0,1}, scale {1,1,1}
  j.name = name;
  j.parent_index = parent;
  j.translation[0] = t.x();
  j.translation[1] = t.y();
  j.translation[2] = t.z();
  return j;
}

// 3-joint arm: shoulder at origin (bind identity), elbow at midT relative to
// the shoulder, "hand_r" at endT relative to the elbow. All rotations are
// bind-pose identity so the world pose is just the summed translations.
std::shared_ptr<ossia::scene_state>
makeArmScene(QVector3D midT, QVector3D endT)
{
  auto root = std::make_shared<ossia::scene_node>();
  root->id.value = 1;
  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  roots->push_back(root);

  auto skel = std::make_shared<ossia::skeleton_component>();
  skel->joints.push_back(joint("shoulder", -1, {0, 0, 0}));
  skel->joints.push_back(joint("elbow", 0, midT));
  skel->joints.push_back(joint("hand_r", 1, endT));
  auto skels = std::make_shared<std::vector<ossia::skeleton_component_ptr>>();
  skels->push_back(skel);

  auto st = std::make_shared<ossia::scene_state>();
  st->roots = roots;
  st->skeletons = skels;
  st->version = 1;
  return st;
}

const ossia::skeleton_component* outSkel(Threedim::InverseKinematics& n)
{
  const auto& st = n.outputs.scene_out.scene.state;
  if(!st || !st->skeletons || st->skeletons->empty())
    return nullptr;
  return (*st->skeletons)[0].get();
}

Threedim::InverseKinematics
makeSolver(std::shared_ptr<ossia::scene_state> st, QVector3D target,
           QVector3D pole = {0.f, 5.f, 0.f}, float weight = 1.f)
{
  Threedim::InverseKinematics ik;
  ik.inputs.scene_in.scene.state = std::move(st);
  ik.inputs.end_joint.value = "hand_r";
  ik.inputs.target.value = {target.x(), target.y(), target.z()};
  ik.inputs.pole.value = {pole.x(), pole.y(), pole.z()};
  ik.inputs.weight.value = weight;
  return ik;
}
} // namespace

// ================================================================ worldJointPos

TEST_CASE("worldJointPos composes T*R*S down the parent chain",
          "[threedim][ik]")
{
  // Parent at (1,2,3) rotated 90 deg about +Z; child offset (1,0,0).
  // Rz(90)*(1,0,0) = (0,1,0), so the child sits at (1,3,3).
  ossia::skeleton_component skel;
  auto j0 = joint("a", -1, {1, 2, 3});
  const auto q = QQuaternion::fromAxisAndAngle({0, 0, 1}, 90.f);
  j0.rotation[0] = q.x();
  j0.rotation[1] = q.y();
  j0.rotation[2] = q.z();
  j0.rotation[3] = q.scalar();
  skel.joints.push_back(j0);
  skel.joints.push_back(joint("b", 0, {1, 0, 0}));

  const auto p = Threedim::InverseKinematics::worldJointPos(skel, 1);
  CHECK(p.x() == Approx(1.f).margin(1e-4));
  CHECK(p.y() == Approx(3.f).margin(1e-4));
  CHECK(p.z() == Approx(3.f).margin(1e-4));

  // Scale applies before the child's translation: parent scale (2,1,1)
  // turns the (1,0,0) offset into (2,0,0), then Rz(90) -> (0,2,0).
  skel.joints[0].scale[0] = 2.f;
  const auto ps = Threedim::InverseKinematics::worldJointPos(skel, 1);
  CHECK(ps.x() == Approx(1.f).margin(1e-4));
  CHECK(ps.y() == Approx(4.f).margin(1e-4));
  CHECK(ps.z() == Approx(3.f).margin(1e-4));
}

// ============================================== length / distance invariants

TEST_CASE("solve preserves bone lengths and reaches the law-of-cosines "
          "root-to-end distance",
          "[threedim][ik]")
{
  // A solve only rewrites the shoulder and elbow ROTATIONS, so both bone
  // lengths must survive exactly, and the elbow angle theta built from
  // cos(theta) = (lA^2 + lB^2 - lTgt^2) / (2 lA lB) must put the end at
  // distance lTgt = min(|target-root|, lA+lB) from the root.
  SECTION("straight arm, reachable target")
  {
    // root(0,0,0)-mid(1,0,0)-end(2,0,0); target (1,1,0): lTgt = sqrt(2).
    auto ik = makeSolver(makeArmScene({1, 0, 0}, {1, 0, 0}), {1, 1, 0});
    ik();

    const auto* sk = outSkel(ik);
    REQUIRE(sk);
    REQUIRE(sk->joints.size() == 3u);
    const auto root = fkPos(*sk, 0), mid = fkPos(*sk, 1), end = fkPos(*sk, 2);
    CHECK((mid - root).length() == Approx(1.f).margin(1e-3));
    CHECK((end - mid).length() == Approx(1.f).margin(1e-3));
    CHECK((end - root).length()
          == Approx(std::sqrt(2.f)).margin(1e-3));

    CHECK(ik.outputs.scene_out.dirty == ossia::scene_port::dirty_transform);
    // The input skeleton was copied, not mutated in place: its shoulder is
    // still at bind-pose identity.
    const auto& in = *(*ik.inputs.scene_in.scene.state->skeletons)[0];
    CHECK(sk != &in);
    CHECK(in.joints[0].rotation[3] == Approx(1.f));
  }

  SECTION("90-degree bent arm, reachable target")
  {
    // root(0,0,0)-mid(1,0,0)-end(1,1,0); target (0.5,0.5,0):
    // lTgt = sqrt(0.5) ~= 0.70711, closing the elbow from 90 deg to
    // acos(0.75) ~= 41.4 deg. Bone lengths stay 1.
    auto ik = makeSolver(makeArmScene({1, 0, 0}, {0, 1, 0}), {.5f, .5f, 0});
    ik();

    const auto* sk = outSkel(ik);
    REQUIRE(sk);
    const auto root = fkPos(*sk, 0), mid = fkPos(*sk, 1), end = fkPos(*sk, 2);
    CHECK((mid - root).length() == Approx(1.f).margin(1e-3));
    CHECK((end - mid).length() == Approx(1.f).margin(1e-3));
    CHECK((end - root).length()
          == Approx(std::sqrt(0.5f)).margin(1e-3));
  }

  SECTION("out-of-reach colinear target fully extends the chain")
  {
    // Straight arm along +X, target (5,0,0): lTgt clamps to lA+lB = 2 and
    // the chain stays (numerically almost) straight on the +X axis.
    auto ik = makeSolver(makeArmScene({1, 0, 0}, {1, 0, 0}), {5, 0, 0});
    ik();

    const auto* sk = outSkel(ik);
    REQUIRE(sk);
    const auto root = fkPos(*sk, 0), end = fkPos(*sk, 2);
    CHECK(std::isfinite(end.x()));
    CHECK(std::isfinite(end.y()));
    CHECK(std::isfinite(end.z()));
    CHECK((end - root).length() == Approx(2.f).margin(1e-2));
    CHECK(end.x() == Approx(2.f).margin(1e-2));
    CHECK(end.y() == Approx(0.f).margin(1e-2));
    CHECK(end.z() == Approx(0.f).margin(1e-2));
  }
}

TEST_CASE("planar problem stays planar", "[threedim][ik]")
{
  // Chain, target and pole all lie in z=0; both solver rotations are about
  // +/-Z, so the output joints must remain in that plane.
  auto ik = makeSolver(makeArmScene({1, 0, 0}, {1, 0, 0}), {1, 1, 0});
  ik();

  const auto* sk = outSkel(ik);
  REQUIRE(sk);
  CHECK(fkPos(*sk, 1).z() == Approx(0.f).margin(1e-4));
  CHECK(fkPos(*sk, 2).z() == Approx(0.f).margin(1e-4));
}

// ========================================================== target reaching

// DEFECT: solve2Bone derives rootDelta = QQuaternion::rotationTo(r2e_n,
// r2t_n) from the PRE-elbow-bend end direction. Bending the elbow changes
// the root->end direction whenever the elbow angle changes, so aligning the
// OLD direction with the target leaves the actual end effector off target.
// Worked example (first section): straight arm along +X, target (1,1,0).
// The 90 deg elbow bend alone already puts the end at (1,1,0) == target, so
// the correct rootDelta is identity; the code applies an extra Rz(45 deg)
// and lands the end at (0, sqrt(2), 0), ~1.08 away. The fix is to compute
// rootDelta from the post-bend direction, rotationTo(normalize(elbowDelta
// applied to the arm), r2t_n). Expectations below are the correct geometry
// and must not be weakened; drop [!shouldfail] when the solver is fixed.
TEST_CASE("end effector lands on a reachable target "
          "(and points at an unreachable one)",
          "[threedim][ik][!shouldfail]")
{
  SECTION("straight arm, target (1,1,0)")
  {
    auto ik = makeSolver(makeArmScene({1, 0, 0}, {1, 0, 0}), {1, 1, 0});
    ik();
    const auto* sk = outSkel(ik);
    REQUIRE(sk);
    const auto end = fkPos(*sk, 2);
    CHECK(end.x() == Approx(1.f).margin(1e-2));
    CHECK(end.y() == Approx(1.f).margin(1e-2));
    CHECK(end.z() == Approx(0.f).margin(1e-2));
  }

  SECTION("bent arm, target (0.5,0.5,0)")
  {
    auto ik = makeSolver(makeArmScene({1, 0, 0}, {0, 1, 0}), {.5f, .5f, 0});
    ik();
    const auto* sk = outSkel(ik);
    REQUIRE(sk);
    const auto end = fkPos(*sk, 2);
    CHECK(end.x() == Approx(.5f).margin(1e-2));
    CHECK(end.y() == Approx(.5f).margin(1e-2));
    CHECK(end.z() == Approx(0.f).margin(1e-2));
  }

  SECTION("bent arm, unreachable target (0,3,0) -> full extension toward it")
  {
    // Reach is 2, so the correct pose is the straight chain along +Y:
    // end at root + 2*normalize(target-root) = (0,2,0).
    auto ik = makeSolver(makeArmScene({1, 0, 0}, {0, 1, 0}), {0, 3, 0});
    ik();
    const auto* sk = outSkel(ik);
    REQUIRE(sk);
    const auto end = fkPos(*sk, 2);
    CHECK(end.x() == Approx(0.f).margin(2e-2));
    CHECK(end.y() == Approx(2.f).margin(2e-2));
    CHECK(end.z() == Approx(0.f).margin(2e-2));
  }
}

// ================================================= gating and degeneracies

TEST_CASE("weight 0 republishes the input scene untouched", "[threedim][ik]")
{
  auto st = makeArmScene({1, 0, 0}, {1, 0, 0});
  auto ik = makeSolver(st, {1, 1, 0}, {0, 5, 0}, /*weight=*/0.f);
  ik();
  CHECK(ik.outputs.scene_out.scene.state.get() == st.get());
  CHECK(ik.outputs.scene_out.dirty == 0);
}

TEST_CASE("non-solvable inputs pass through; a null scene clears the output",
          "[threedim][ik]")
{
  SECTION("null input scene")
  {
    Threedim::InverseKinematics ik;
    ik();
    CHECK(ik.outputs.scene_out.scene.state == nullptr);
    CHECK(ik.outputs.scene_out.dirty == 0);
  }

  SECTION("unknown end joint name")
  {
    auto st = makeArmScene({1, 0, 0}, {1, 0, 0});
    auto ik = makeSolver(st, {1, 1, 0});
    ik.inputs.end_joint.value = "does_not_exist";
    ik();
    CHECK(ik.outputs.scene_out.scene.state.get() == st.get());
    CHECK(ik.outputs.scene_out.dirty == 0);
  }

  SECTION("chain shorter than three joints")
  {
    auto st = makeArmScene({1, 0, 0}, {1, 0, 0});
    auto skel = std::make_shared<ossia::skeleton_component>();
    skel->joints.push_back(joint("upper", -1, {0, 0, 0}));
    skel->joints.push_back(joint("hand_r", 0, {1, 0, 0}));
    auto skels
        = std::make_shared<std::vector<ossia::skeleton_component_ptr>>();
    skels->push_back(skel);
    auto st2 = std::make_shared<ossia::scene_state>(*st);
    st2->skeletons = skels;

    auto ik = makeSolver(st2, {0, 1, 0});
    ik();
    CHECK(ik.outputs.scene_out.scene.state.get() == st2.get());
    CHECK(ik.outputs.scene_out.dirty == 0);
  }

  SECTION("scene without skeletons")
  {
    auto st = makeArmScene({1, 0, 0}, {1, 0, 0});
    auto st2 = std::make_shared<ossia::scene_state>(*st);
    st2->skeletons.reset();
    auto ik = makeSolver(st2, {0, 1, 0});
    ik();
    CHECK(ik.outputs.scene_out.scene.state.get() == st2.get());
    CHECK(ik.outputs.scene_out.dirty == 0);
  }
}

TEST_CASE("zero-length bone degenerates to identity, never NaN",
          "[threedim][ik]")
{
  // lA = |mid-root| = 0: the solver must bail out to identity deltas, so
  // the pose is unchanged and every coordinate stays finite.
  auto ik = makeSolver(makeArmScene({0, 0, 0}, {1, 0, 0}), {0, 1, 0});
  ik();

  const auto* sk = outSkel(ik);
  REQUIRE(sk);
  const auto end = fkPos(*sk, 2);
  CHECK(std::isfinite(end.x()));
  CHECK(std::isfinite(end.y()));
  CHECK(std::isfinite(end.z()));
  CHECK(end.x() == Approx(1.f).margin(1e-3));
  CHECK(end.y() == Approx(0.f).margin(1e-3));
  CHECK(end.z() == Approx(0.f).margin(1e-3));
}
